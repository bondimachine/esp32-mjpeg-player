# ESP32 composite video out remote MJPEG player


This code allows to reproduce a video on a remote screen using an ESP32.

Composite video out goes thru pin 25. 

Then you can send video to the ESP using ffmpeg like

```
ffmpeg -re -i ~/Downloads/never.mp4 -f image2 -c:v mjpeg -update 1 -vf "crop=ih/15*16:ih,scale=w=256:h=240" -r 25 udp://192.168.100.85:1234
```

