//
// Created by frank on 2022/1/6.
//

#include<stdio.h>
#include <stdint.h>
#include <unistd.h>

extern "C"
{
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

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
    if (!packetList.empty()) {
        auto p = packetList.front();
        memcpy(stream, (uint8_t *) p.pBuf, p.size);
        delete[] p.pBuf;
        packetList.pop_front();
    }
    mtxList.unlock();
}

extern int optind, opterr, optopt;
extern char *optarg;

int main(int argc, char *argv[]) {

    FILE *wavFd;
    SDL_AudioSpec spec;
    int more_chunks = 1;
    struct riff_wave_header riff_wave_header{};
    struct chunk_header chunk_header{};
    struct chunk_fmt chunk_fmt{};

    std::string file;
    bool bProcess = false;

    enum AVSampleFormat srcSampleFmt = AV_SAMPLE_FMT_S16;
    enum AVSampleFormat dstSampleFmt = AV_SAMPLE_FMT_FLT;
    struct SwrContext *pSwrCtx = nullptr;


    if (argc < 2) {
        printf("\n\n/Usage:./main -f xxxx.wav/\n");
    }

    int c = 0; //用于接收选项
    /*循环处理参数*/
    while (EOF != (c = getopt(argc, argv, "pf:"))) {
        //打印处理的参数
        printf("start to process %d para\n", optind);
        switch (c) {
            case 'p':
                printf("we get option -p\n");
                bProcess = true;
                break;
                //-n选项必须要参数
            case 'f':
                printf("we get option -n,para is %s\n", optarg);
                file = std::string(optarg);
                break;
                //表示选项不支持
            case '?':
                printf("unknow option:%c\n", optopt);
                break;
            default:
                break;
        }
    }
    //初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        printf("Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }

    //打开wav文件
    wavFd = fopen(file.c_str(), "rb");
    if (!wavFd) {
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

    spec.format = AUDIO_S16SYS;
    spec.channels = chunk_fmt.num_channels;
    spec.silence = 0;
    spec.samples = 256;
    spec.callback = consume_audio_data;//回调函数由SDL内部线程调用
    spec.userdata = nullptr;
    printf("spec.channels=%d,spec.freq=%d, chunk_fmt.bits_per_sample=%d\n", spec.channels, spec.freq,
           chunk_fmt.bits_per_sample);


    uint32_t size = 0;
    size = chunk_fmt.bits_per_sample / 8 * chunk_fmt.num_channels * spec.samples;


    uint8_t **ppSrc = NULL, **ppDst = NULL;
    uint8_t *pSrc = NULL;
    uint8_t *pDst = NULL;
    int srcSize = 0;
    int dtsSize = 0;
    int src_linesize, dst_linesize;
    int src_nb_channels = chunk_fmt.num_channels, dst_nb_channels = chunk_fmt.num_channels;
    int src_nb_samples = spec.samples, dst_nb_samples = spec.samples;
    if (bProcess) {
        pSwrCtx = swr_alloc();
        // 我们只转换位深
        av_opt_set_int(pSwrCtx, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0);
        av_opt_set_int(pSwrCtx, "in_sample_rate", chunk_fmt.sample_rate, 0);
        av_opt_set_sample_fmt(pSwrCtx, "in_sample_fmt", srcSampleFmt, 0);
        av_opt_set_sample_fmt(pSwrCtx, "out_sample_fmt", dstSampleFmt, 0);
        av_opt_set_int(pSwrCtx, "out_sample_rate", chunk_fmt.sample_rate, 0);
        av_opt_set_int(pSwrCtx, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);

        if ((swr_init(pSwrCtx)) < 0) {
            fprintf(stderr, "Failed to initialize the resampling context\n");
            return -1;
        }

        spec.format = AUDIO_F32SYS; //AUDIO_S32SYS;

        // 根据产生分配内存用于存储音频
        int ret = av_samples_alloc_array_and_samples(&ppSrc, &src_linesize, src_nb_channels,
                                                     src_nb_samples, srcSampleFmt, 0);
        if (ret < 0) {
            fprintf(stderr, "Could not allocate source samples\n");
            return -1;
        }

        pSrc = *ppSrc;
        srcSize = ret;

        printf("src data size : %d\n", ret);

        ret = av_samples_alloc_array_and_samples(&ppDst, &dst_linesize, dst_nb_channels,
                                                 dst_nb_samples, dstSampleFmt, 0);
        if (ret < 0) {
            fprintf(stderr, "Could not allocate destination samples\n");
            return -1;
        }
        pDst = *ppDst;
        dtsSize = ret;

        printf("dts data size : %d\n", ret);
    }


    //打开sdl audio设备
    if (SDL_OpenAudio(&spec, nullptr)) {
        fprintf(stderr, "Failed to open audio device, %s\n", SDL_GetError());
        return -1;
    }

    //0:播放，1:暂停
    SDL_PauseAudio(0);

    auto* pBuf = new int8_t[size];
    int readSize = 0;
    while(readSize = fread(pBuf, 1,size,wavFd ), readSize > 0) {
        int dataSize = readSize;
        int8_t *pData = nullptr;

        if (!bProcess) {
            pData = new int8_t[readSize];
            memcpy(pData, pBuf, readSize);
        } else {

            if (readSize > srcSize) {
                printf("readSize:%d > srcSize:%d\n", readSize, srcSize);
                return -1;
            }

            printf("Playing AV_SAMPLE_FMT_FLT\n");
            // 获取输入音频数据
            memcpy(pSrc, pBuf, readSize);

            // 转换
            src_nb_samples = readSize / (chunk_fmt.num_channels * chunk_fmt.bits_per_sample / 8);
            dst_nb_samples = src_nb_samples;
            auto result = swr_convert(pSwrCtx, ppDst, dst_nb_samples, (const uint8_t **) ppSrc, src_nb_samples);
            if (result < 0) {
                fprintf(stderr, "Error while converting\n");
                return -1;
            }
            int dstBufsize = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels,
                                                        result, dstSampleFmt, 1);
            if (dstBufsize < 0) {
                fprintf(stderr, "Could not get sample buffer size\n");
                return -1;
            }


            // 拷贝输出数据,这个可以直接播放
            pData = new int8_t[dstBufsize];
            memcpy(pData, pDst, dstBufsize);
            dataSize = dstBufsize;

        }

        mtxList.lock();
        packetList.emplace_back(pData, dataSize);
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