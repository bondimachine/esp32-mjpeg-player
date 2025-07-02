# ESP32 composite video out remote MJPEG player


This code allows to reproduce a video on a remote screen using an ESP32 connected thru composite video.

Composite video out goes thru pin 25.

Then you can send video to the ESP using ffmpeg like

```
ffmpeg -re -i ~/Downloads/never.mp4 -f image2 -c:v mjpeg -pix_fmt yuv420p -update 1 -vf "crop=ih/3*4:ih,scale=w=320:h=240" -r 25 udp://192.168.100.85:1234
```

