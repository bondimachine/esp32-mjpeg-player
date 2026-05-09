#include "config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include "font.h"

#include <esp_task_wdt.h>

AsyncUDP udp;

#include "video_out.h"

#define PER_FRAME_DELAY_MS 40 // 25 fps

struct PacketBuffer {
  uint8_t* data;
  uint16_t size;
};


#ifndef MPEG
#include <JPEGDEC.h>
JPEGDEC jpeg;

#define BUFFERED_FRAMES 20
#define RAW_FRAME_BUFFER_FRAMES 16
#define MAX_FRAME_BUFFER 4000

uint8_t rawFrameBuffer[MAX_FRAME_BUFFER * RAW_FRAME_BUFFER_FRAMES];

struct PacketBuffer buffer[BUFFERED_FRAMES];
uint32_t readIndex = 0;
volatile uint32_t writeIndex = 0;
bool discard = false;

#else

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

#include "tsdemux.h"

#define BUFFER_SIZE 1024 * 4

plm_buffer_t * videoBuffer;
plm_video_t * videoDecoder;

PacketBuffer rest;

TSDemuxContext ctx;

#endif 

#include "palette.h"

#define WIDTH 320
#define HEIGHT 240

void on_frame() {
}

#ifndef MPEG
int drawMCUs(JPEGDRAW *pDraw) {

  if (pDraw->x > WIDTH || pDraw->y > HEIGHT) {
    return 0;
  }

  int xLimit = min(pDraw->iWidth + pDraw->x, WIDTH) - pDraw->x;
  int yLimit = min(pDraw->iHeight + pDraw->y, HEIGHT);

  // Serial.printf("Draw pos = %d,%d. size = %d x %d\n", pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);

  for (int y = pDraw->y, row = 0; y < yLimit; y++, row++) {
    uint8_t* bufferLine = ((uint8_t*)pDraw->pPixels) + row * pDraw->iWidth;
    #ifndef ROTATE_180
      memcpy(&_lines[y][pDraw->x], bufferLine, xLimit);
    #else
      for(int col = 0; col < xLimit; col++) {
        _lines[HEIGHT - 1 - y][WIDTH - 1 - pDraw->x - col] = bufferLine[col];
      }
    #endif
  }

  return 1; // returning true (1) tells JPEGDEC to continue decoding. Returning false (0) would quit decoding immediately.
}

int eoi(uint8_t* data, uint16_t size) {
  if (size >= 2) {
    for(int i=0; i<size-1; i++) {
      if(data[i] == 0xFF && data[i+1] == 0xD9) {
        return i+2;
      }
    }  
  }
  return -1;
}

void onMJPEGPacket(AsyncUDPPacket packet) {

  PacketBuffer* currentBuffer = &buffer[writeIndex % BUFFERED_FRAMES];
  int split = eoi(packet.data(), packet.length());

  int packet_length = packet.length();
  if (split != -1) {
    packet_length = split;
  }
  if (!discard && ((currentBuffer->size + packet_length) < MAX_FRAME_BUFFER)) {
    memcpy(&currentBuffer->data[currentBuffer->size], packet.data(), packet_length);
    currentBuffer->size += packet_length;
  } else {
    Serial.print("buffer exceeded - ignoring packet ");
    Serial.println(currentBuffer->size);
    discard = (split == -1); // discard until we find the end of this frame
  }
  if (split != -1) {
    writeIndex++;
    // assuming always packet size < frame buffer size
    currentBuffer = &buffer[writeIndex % BUFFERED_FRAMES];
    currentBuffer->size = packet.length() - split;
    if (currentBuffer->size > 0) {
      memcpy(&currentBuffer->data, packet.data() + split, currentBuffer->size);
      Serial.println("split packet ");
    }
  }
}

#else 

void onTsd(TSDemuxContext *ctx, uint16_t pid, TSDEventId event_id, void *data) {
    if (event_id == TSD_EVENT_PMT) {
      TSDPMTData *pmt = (TSDPMTData*) data;
      for(int i=0;i<pmt->program_elements_length; ++i) {
        TSDProgramElement *prog = &pmt->program_elements[i];
        // printf("PMT pid: 0x%x, stream_type: 0x%0x\n", prog->elementary_pid, prog->stream_type);
        if (prog->stream_type == TSD_PMT_STREAM_TYPE_VIDEO || prog->stream_type == TSD_PMT_STREAM_TYPE_VIDEO_H262) {
          tsd_register_pid(ctx, prog->elementary_pid, TSD_REG_PES);
        }
      };
    } else if(event_id == TSD_EVENT_PES) {
        TSDPESPacket *pes = (TSDPESPacket*) data;
        // This is where we would write the PES data into our buffer.
        // Serial.printf("\n====================\n");
        // Serial.printf("PID 0x%x PES Packet, Size: %d, stream_id=0x%x, pts=%llu, dts=%llu\n", pid, pes->data_bytes_length, pes->stream_id, pes->pts, pes->dts);
        
    }

}


#endif

void print(String s) {
  for(int x = 0; x < s.length(); x++) {
    #ifndef ROTATE_180  
      draw_char(_lines, s[x], x+5, 15, 254, 0);
    #else
      draw_char(_lines, s[x], (WIDTH/6) - 5 - x, (HEIGHT/8) - 15, 254, 0);
    #endif
  }
}


void setup() {
    setCpuFrequencyMhz(240);
    Serial.begin(115200);

    Serial.print("PSRAM ");
    Serial.println(ESP.getPsramSize());

    uint8_t* _front_buffer = (uint8_t*)calloc(HEIGHT*WIDTH, 1);
    _lines = (uint8_t**) malloc(HEIGHT * sizeof(uint8_t*));
    for (int y = 0; y < HEIGHT; y++) {
      _lines[y] = _front_buffer + y*WIDTH;
    }

#ifndef MPEG
    for(int i = 0; i < BUFFERED_FRAMES; i++) {
      if (i < RAW_FRAME_BUFFER_FRAMES) {
        buffer[i].data = &rawFrameBuffer[i * MAX_FRAME_BUFFER];
      } else {
        buffer[i].data = (uint8_t*) malloc(MAX_FRAME_BUFFER);
        if (!buffer[i].data) {
          Serial.print("failed to allocate frame buffer ");
          Serial.println(i);
        }
      }
      buffer[i].size = 0;
    }

    udp.onPacket(onMJPEGPacket);
#else

    // videoBuffer = plm_buffer_create_with_capacity(BUFFER_SIZE);
    // if (!videoBuffer) {
    //   Serial.println("Failed to create buffer");
    // }

		// videoDecoder = plm_video_create_with_buffer(videoBuffer, TRUE);

    // if (!videoDecoder) {
    //   Serial.println("Failed to create video decoder instance");;
    // }

    tsd_context_init(&ctx);

    tsd_set_event_callback(&ctx, onTsd);

    rest.data = (uint8_t*)malloc(TSD_TSPACKET_SIZE);
    rest.size = 0;

    udp.onPacket([](AsyncUDPPacket packet) {

      size_t parsed = 0; // number of bytes parsed by the demuxer.

      size_t to_skip = 0;

      if (rest.size > 0) {
        to_skip = TSD_TSPACKET_SIZE - rest.size;
        if (packet.length() >= to_skip) {
          memcpy(rest.data + rest.size, packet.data(), to_skip);   
          TSDCode res = tsd_demux(&ctx, rest.data, TSD_TSPACKET_SIZE, &parsed);
          rest.size = 0;
          if (res) {
            Serial.printf("Error decoding rest %d\n", res);
          }
          if (parsed != TSD_TSPACKET_SIZE) {
            Serial.printf("Rest non parsed? %d\n", parsed);
          }
        }
      }

      if (packet.length() < to_skip) {
        Serial.printf("packet too small %d", packet.length());
        memcpy(rest.data + rest.size, packet.data(), packet.length());
        rest.size += packet.length();
      } else if (packet.length() == to_skip) {
        // we consumed it all!
        return;
      }

      
      TSDCode res = tsd_demux(&ctx, packet.data() + to_skip, packet.length() - to_skip, &parsed);

      if (res) {
        Serial.printf("TS demux error %d\n", res);
        rest.size = 0; // discard all this thing
      } else {
        rest.size = packet.length() - to_skip - parsed;
        if (rest.size > TSD_TSPACKET_SIZE) {
          Serial.printf("TS demux rest too big? %d\n", rest.size);
          rest.size = 0; // discard...
        } else if (rest.size > 0) {
          memcpy(rest.data, packet.data() + to_skip + parsed, rest.size);
        }
      }


      // plm_buffer_write(inputBuffer, packet.data(), packet.length());

    });
#endif 

    print("Connecting...");

    video_init(blacknwhite_4_phase_pal, 256, false);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    Serial.print("Connecting");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");

    if (udp.listen(PORT)) {
      Serial.print("WiFi connected. UDP Listening on IP: ");
      String ip = WiFi.localIP().toString();

      Serial.println(ip);

      print(ip);

      xTaskCreatePinnedToCore(render, "render", 1024, NULL, 1, NULL, 0);

    } else {
      Serial.println("failed to listen");
    }
}

void loop() {
}

void render(void* ignored) {

  esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(0));

  Serial.println("render started");

  unsigned long lastFrame = 0;

  for(;;) {
    unsigned long now = millis();

#ifndef MPEG
    if (readIndex < writeIndex && ((now - lastFrame) >= PER_FRAME_DELAY_MS)) {  // force fps
      PacketBuffer* currentBuffer = &buffer[readIndex % BUFFERED_FRAMES];
      // if (jpeg.openFLASH((uint8_t *)sample_image, sizeof(sample_image), drawMCUs)) {
      if (jpeg.openRAM(currentBuffer->data, currentBuffer->size, drawMCUs)) {
        jpeg.setPixelType(EIGHT_BIT_GRAYSCALE);
        if (!jpeg.decode(0, 0, JPEG_LUMA_ONLY)) {
          Serial.println("failed to decode");
        }
        jpeg.close();
      } else {
        Serial.println("failed to open");
      }
      readIndex++;
#else
    if ((now - lastFrame) >= PER_FRAME_DELAY_MS) {
    //   plm_decode(plm, 0.04);
#endif

      lastFrame = now;
    }
  }
}