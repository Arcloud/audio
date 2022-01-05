#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <string>
#include <list>
#include <mutex>

int readPCM(FILE* pFile, int8_t* pBuf, uint32_t size);


typedef struct Packet {
    int8_t* pBuf;
    uint32_t  size;
    Packet(int8_t* pBuf, uint32_t size) : pBuf(pBuf), size(size) {};
}Packet;

std::list<Packet> packetList;
std::mutex mtxList;

void  fill_audio(void *udata,Uint8 *stream,int len){
    mtxList.lock();
    if(!packetList.empty()) {
        auto p = packetList.front();
        memcpy(stream, (uint8_t *)p.pBuf, p.size);
        delete [] p.pBuf;
        packetList.pop_front();
    }
    mtxList.unlock();
}

int main(int argc, char * argv[])
{
    std::string fileName;

    if(argc == 2) {
        fileName = std::string(argv[1]);
    }else {
        printf("参数错误\n");
        return -1;
    }


    FILE* pFile = fopen(fileName.c_str(), "rb+");
    if(pFile == nullptr) {
        printf("load pcm file %s failed\n", fileName.c_str());
        return -1;
    }else {
        printf("load pcm file %s succeed\n", fileName.c_str());
    }


    if(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        printf( "Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }

    int channel = 8;
    int samples =  128;
    int bufSize = 2 * channel * samples ;

    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = 48000;
    wanted_spec.format = AUDIO_S32SYS;
    wanted_spec.channels = channel;
    wanted_spec.silence = 0;
    wanted_spec.samples = samples;
    wanted_spec.callback = fill_audio;

    if (SDL_OpenAudio(&wanted_spec, nullptr)<0){
        printf("can't open audio.\n");
        return -1;
    }

    SDL_PauseAudio(0);

    auto* pBuf = new int8_t[bufSize];
    int readSize = 0;

    while(readSize = readPCM(pFile, pBuf, bufSize), readSize > 0 ) {
        samples = readSize / (channel*2);
        printf("readSize : %d, channel: %d, samples : %d\n",
               readSize, channel, samples);

        int8_t* pData = nullptr;
        uint32_t  size = 0;

        pData = new int8_t [readSize];
        size = readSize;
        memcpy(pData,pBuf, readSize);

        mtxList.lock();
        packetList.emplace_back(pData, size);
        mtxList.unlock();

        SDL_Delay(2);
    }

    delete [] pBuf;
    fclose(pFile);


    return 0;
}


int readPCM(FILE* pFile, int8_t* pBuf, uint32_t size){
    return fread(pBuf,1,size,pFile);
}