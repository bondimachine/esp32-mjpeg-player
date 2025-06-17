#include <stdio.h>
#include <stdint.h>
#include <math.h>

#define IRE(_x)          ((uint32_t)(((_x)+40)*255/3.3/147.5) << 8)   // 3.3V DAC
#define SYNC_LEVEL       IRE(-40)
#define BLANKING_LEVEL   IRE(0)
#define BLACK_LEVEL      IRE(7.5)
#define GRAY_LEVEL       IRE(50)
#define WHITE_LEVEL      IRE(100)

int main() {
    uint16_t len = 256;
    uint32_t pal[256*2];
    uint32_t* even = pal;
    uint32_t* odd = pal + len;

    float chroma_scale = BLANKING_LEVEL/2/256;
    //chroma_scale /= 127;  // looks a little washed out
    chroma_scale /= 80;
    for (int i = 0; i < 255; i++) {

        float y = i;
        float u = 0;
        float v = 0;
        y /= 255.0;
        y = (y*(WHITE_LEVEL-BLACK_LEVEL) + BLACK_LEVEL)/256;

        uint32_t e = 0;
        uint32_t o = 0;
        for (int i = 0; i < 4; i++) {
            float p = 2*M_PI*i/4 + M_PI;
            float s = sin(p)*chroma_scale;
            float c = cos(p)*chroma_scale;
            uint8_t e0 = round(y + (s*u) + (c*v));
            uint8_t o0 = round(y + (s*u) - (c*v));
            e = (e << 8) | e0;
            o = (o << 8) | o0;
        }
        *even++ = e;
        *odd++ = o;
    }

    printf("uint32_t blacknwhite_4_phase_pal[] = {\n");
    for (int i = 0; i < len*2; i++) {  // start with luminance map
        printf("0x%08X,",pal[i]);
        if ((i & 7) == 7)
            printf("\n");
        if (i == (len-1)) {
            printf("//odd\n");
        }
    }
    printf("};\n");
}