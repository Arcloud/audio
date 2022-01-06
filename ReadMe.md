## 音频测试项目


### 使用ffplayer播放pcm数据
```shell
# 双声道
ffplay -ar 48000 -channels 2 -f s16le -i 48000_16_2_output.pcm

# 7.1
ffplay -ar 48000 -channels 8 -f s16le -i  audio_8channels.pcm
```


#### 转换位深
```shell
#-i:  输入文件
#-vn: 输出文件禁用视频流
#-ac: 设置输出文件音频通道的数量
#-ar: 设置输出文件音频采样率
#-acodec: 设置输出文件音频编解码器
#-y: 无需询问即可覆盖输出文件
ffmpeg -i music_orig_MultDmix6Ch.wav -vn -ac 6 -ar 48000 -acodec pcm_s32le -y music_orig_MultDmix6Ch_32bits.wav


```

### 例子说明
+ sdl 播放双声道PCM
+ sdl_8_channel 播放8声道PCM
