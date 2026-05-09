# ESP32 composite video out remote MJPEG & MPEG1 player


This code allows to reproduce a video on a remote screen using an ESP32 connected thru composite video.

Composite video out goes thru pin 25.

Then you can send video to the ESP using ffmpeg like (mjpeg mode)

```
ffmpeg -re -i ~/Downloads/never.mp4 -f image2 -c:v mjpeg -pix_fmt yuv420p -update 1 -vf "crop=ih/3*4:ih,scale=w=320:h=240" -r 25 udp://192.168.100.85:1234
```

OR (mpeg1 mode)

```
ffmpeg -re -i ~/Downloads/never.mp4 -f mpegts -c:v mpeg1video -pix_fmt yuv420p -update 1 -vf "crop=ih/3*4:ih,scale=w=320:h=240" -an -r 25 udp://192.168.100.85:1234
```

If you need to keep sync, you can also use mjpeg over mpegts if your ESP doesn't have external PSRAM to support mpeg1

```
ffmpeg -re -i ~/Downloads/never.mp4 -f mpegts -c:v mjpeg -pix_fmt yuv420p -update 1 -vf "crop=ih/3*4:ih,scale=w=320:h=240" -an -r 25 udp://192.168.100.85:1234
```


Many thanks to BitBank2 for thier mpeg and jpeg libraries! 



