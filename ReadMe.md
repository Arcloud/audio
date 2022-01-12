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
+ sdl_8ch_32bits 播放8省道，位深为32bits的PCM
+ sdl_wav 播放WAV文件


### 工具
+ [audacity](https://github.com/audacity/audacity/releases) 音频

### 参考资料
+ [《FFMPEG开发快速入坑——音频转换处理》](https://zhuanlan.zhihu.com/p/345880400)
+ [libfar](https://github.com/feixiao/libfar) C/C++ fast audio resampling library.
+ [r8brain-free-src](https://github.com/feixiao/r8brain-free-src) High-quality pro audio resampler / sample rate converter C++ library. 