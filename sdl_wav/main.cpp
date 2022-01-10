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
#include <list>
#include <mutex>


typedef struct Packet {
    int8_t* pBuf;
    uint32_t  size;
    Packet(int8_t* pBuf, uint32_t size) : pBuf(pBuf), size(size) {};
}Packet;

std::list<Packet> packetList;
std::mutex mtxList;


//#define BUFFER_SIZE 81920

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


int32_t from3Bytes(const int8_t* pBuf)
{
    int32_t result = 0;
    result += pBuf[0];
    result += pBuf[1] << 8;
    result += pBuf[2] << 16;
    return result;
}


int32_t from4Bytes(const int8_t* pBuf)
{
    return * ((int32_t*)(pBuf));
}


void consume_audio_data(void *udata, Uint8 *stream, int len){

    mtxList.lock();
    if(!packetList.empty()) {
        auto p = packetList.front();
        memcpy(stream, (uint8_t *)p.pBuf, p.size);
        delete [] p.pBuf;
        packetList.pop_front();
    }
    mtxList.unlock();
}

int main(int argc, char *argv[]){

    FILE *wavFd;
    SDL_AudioSpec spec;
    int more_chunks = 1;
    struct riff_wave_header riff_wave_header{};
    struct chunk_header chunk_header{};
    struct chunk_fmt chunk_fmt{};

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
        return -1;
    }

    // 读取wav文件头
    fread(&riff_wave_header, sizeof(riff_wave_header), 1, wavFd);

    if ((riff_wave_header.riff_id != ID_RIFF) ||
        (riff_wave_header.wave_id != ID_WAVE)) {
        fprintf(stderr, "Error: '%s' is not a riff/wave file\n", argv[1]);
        fclose(wavFd);
        return -1;
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

    printf("sample_rate:%d  num_channels:%d bits_per_sample:%d\n",
           chunk_fmt.sample_rate,
           chunk_fmt.num_channels,
           chunk_fmt.bits_per_sample);

    // 填充结构体各字段
    spec.freq = chunk_fmt.sample_rate;



    spec.channels = chunk_fmt.num_channels;
    spec.silence = 0;
    spec.samples = 256;
    spec.callback = consume_audio_data;//回调函数由SDL内部线程调用
    spec.userdata = nullptr;
    printf("spec.channels=%d,spec.freq=%d, chunk_fmt.bits_per_sample=%d\n", spec.channels, spec.freq, chunk_fmt.bits_per_sample);


    uint32_t  size = 0;
    switch (chunk_fmt.bits_per_sample){

        case 16:
            spec.format = AUDIO_S16SYS; //AUDIO_U16SYS;
            break;
        case 24:
        case 32:
            spec.format = AUDIO_S32SYS; //AUDIO_S32SYS;
            break;
        default:
            printf("not support this format, bits_per_sample:%d\n", chunk_fmt.bits_per_sample);
            return -1;
    }

    size = chunk_fmt.bits_per_sample/8 * chunk_fmt.num_channels * spec.samples;

    //打开sdl audio设备
    if(SDL_OpenAudio(&spec, NULL)){
        fprintf(stderr, "Failed to open audio device, %s\n", SDL_GetError());
        return -1;
    }

    //0:播放，1:暂停
    SDL_PauseAudio(0);


    auto* pBuf = new int8_t[size];
    int readSize = 0;
    while(readSize = fread(pBuf, 1,size,wavFd ), readSize > 0) {

        int8_t* pData = nullptr;

        if(chunk_fmt.bits_per_sample != 24) {
            pData = new int8_t [readSize];
            memcpy(pData,pBuf, readSize);


            
            int bytesPerSample = chunk_fmt.bits_per_sample / 8;
            int numSamples = readSize/(bytesPerSample * chunk_fmt.num_channels);
            int8_t* p = new int8_t[readSize/chunk_fmt.num_channels];


            for(int i=0; i < numSamples; i++) {
                int step1 = i * bytesPerSample * chunk_fmt.num_channels;
                int step2 = i * bytesPerSample; // 一个声道
                // FILE* pFile= fopen("./wav_l.pcm", "ab+");
                // if(pFile != nullptr) {
                //     // fwrite(pSample+step, 2, 1, pFile);
                //     fclose(pFile);
                // }

                memcpy(p+step2, pData+step1, bytesPerSample);
            }

            // ffplay -ar 48000 -channels 1 -f s16le -i wav_l.pcm
            FILE* pFile= fopen("./wav_l.pcm", "ab+");
            if(pFile != nullptr) {
                fwrite(p, 1, readSize/chunk_fmt.num_channels, pFile);
                fclose(pFile);
            }

            delete [] p;

        } else {
            // 3 个字节一个单元
            int unit = readSize/3;

            // 24 bits  转 32bit
            auto p = new uint32_t [unit];

            for(int i =0; i< unit; i++) {

                uint32_t  volume = from3Bytes(pBuf+ 3*i);

                p[i] = volume;

                uint32_t  volume2 = from4Bytes(reinterpret_cast<const int8_t *>(p + i));
                if (volume != volume2) {
                    printf("%d != %d \n", volume, volume2);
                }
//                printf("%u = %u \n", volume, volume2);
            }

            readSize = 4*unit;
            pData = reinterpret_cast<int8_t *>(p);
        }

        mtxList.lock();
        packetList.emplace_back(pData, readSize);
        mtxList.unlock();

        SDL_Delay(2);
    }


    SDL_CloseAudio();

    delete [] pBuf;
    if(wavFd){
        fclose(wavFd);
    }

    SDL_Quit();
}