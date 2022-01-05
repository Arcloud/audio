## 音频测试项目


#### 使用ffplayer播放pcm数据
```shell
# 双声道
ffplay -ar 48000 -channels 2 -f s16le -i 48000_16_2_output.pcm

# 7.1
ffplay -ar 48000 -channels 8 -f s16le -i  audio_8channels.pcm
```