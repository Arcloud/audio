//
// Created by frank on 2022/1/6.
//

#include<stdio.h>
#include <stdint.h>
// #include <endian.h>

// #include <machine/endian.h>
#include<SDL2/SDL.h>

#include <pthread.h>

// https://stackoverflow.com/questions/20813028/endian-h-not-found-on-mac-osx
#ifndef __APPLE__
    #warning "This header file (endian.h) is MacOS X specific.\n"
#endif  /* __APPLE__ */


#include <libkern/OSByteOrder.h>

#define BUFFER_SIZE 81920

#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164

struct riff_wave_header {
    uint32_t riff_id;
    uint32_t riff_sz;
    uint32_t wave_id;
};

struct chunk_header {
    uint32_t id;
    uint32_t sz;
};

struct chunk_fmt {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};


static Uint8      *data_buf       = NULL;
static Uint8      *consume_pos    = NULL;
static uint32_t   data_len        = 0;

void consume_audio_data(void *udata, Uint8 *stream, int len){

    if(data_len == 0){
        printf("consume_audio_data data_len == 0 return \n");
        return;
    }

    SDL_memset(stream, 0, len);

    len = (len < data_len) ? len : data_len;
    SDL_MixAudio(stream, consume_pos, len, SDL_MIX_MAXVOLUME);
    consume_pos += len;
}

int main(int argc, char *argv[]){

    FILE *wavFd = NULL;
    SDL_AudioSpec spec;
    int more_chunks = 1;
    struct riff_wave_header riff_wave_header;
    struct chunk_header chunk_header;
    struct chunk_fmt chunk_fmt;

    if(argc < 2) {
        printf("\n\n/Usage:./main xxxx.wav/\n");
    }
    //初始化SDL
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)){
        printf("Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }

    //打开wav文件
    wavFd = fopen(argv[1], "rb");
    if(!wavFd){
        fprintf(stderr, "Failed to open pcm file!\n");
        goto __FAIL;
    }

    //读取wav文件头
    fread(&riff_wave_header, sizeof(riff_wave_header), 1, wavFd);

    if ((riff_wave_header.riff_id != ID_RIFF) ||
        (riff_wave_header.wave_id != ID_WAVE)) {
        fprintf(stderr, "Error: '%s' is not a riff/wave file\n", argv[1]);
        fclose(wavFd);
        goto __FAIL;
    }

    do {
        fread(&chunk_header, sizeof(chunk_header), 1, wavFd);

        switch (chunk_header.id) {
            case ID_FMT:
                fread(&chunk_fmt, sizeof(chunk_fmt), 1, wavFd);
                if (chunk_header.sz > sizeof(chunk_fmt))
                    fseek(wavFd, chunk_header.sz - sizeof(chunk_fmt), SEEK_CUR);
                break;
            case ID_DATA:
                more_chunks = 0;
                chunk_header.sz = OSSwapLittleToHostInt64(chunk_header.sz);
                break;
            default:
                fseek(wavFd, chunk_header.sz, SEEK_CUR);
        }
    } while (more_chunks);

    //分配buffer
    data_buf = (Uint8*)malloc(BUFFER_SIZE);
    if(!data_buf){
        printf("malloc data_buf fail!\n");
        goto __FAIL;
    }
    //填充结构体各字段
    spec.freq = chunk_fmt.sample_rate;

    switch (chunk_fmt.bits_per_sample){

        case 16:
            spec.format = AUDIO_S16SYS;//AUDIO_U16SYS;
            break;

        case 32:
            spec.format = AUDIO_S32SYS;
            break;

        case 24:
            spec.format = AUDIO_S32SYS;
            break;

        default:
            printf("not support this format, bits_per_sample:%d\n", chunk_fmt.bits_per_sample);
            goto __FAIL;
    }

    spec.channels = chunk_fmt.num_channels;
    spec.silence = 0;
    spec.samples = 2048;
    spec.callback = consume_audio_data;//回调函数由SDL内部线程调用
    spec.userdata = NULL;
    printf("spec.channels=%d,spec.freq=%d, chunk_fmt.bits_per_sample=%d\n", spec.channels, spec.freq, chunk_fmt.bits_per_sample);
    //打开sdl audio设备
    if(SDL_OpenAudio(&spec, NULL)){
        fprintf(stderr, "Failed to open audio device, %s\n", SDL_GetError());
        goto __FAIL;
    }

    //0:播放，1:暂停
    SDL_PauseAudio(0);
    do{
        //读取pcm数据
        data_len = fread(data_buf, 1, BUFFER_SIZE, wavFd);
        //  fprintf(stderr, "fread data_len = %d\n", data_len);
        consume_pos = data_buf;
        //sdl内部线程还没有播放完数据，这里延时等待
        while(consume_pos < (data_buf + data_len)) {
            SDL_Delay(1);
        }

    }while(data_len !=0);

    SDL_CloseAudio();

    __FAIL:
    if(data_buf){
        free(data_buf);
    }

    if(wavFd){
        fclose(wavFd);
    }

    SDL_Quit();
}