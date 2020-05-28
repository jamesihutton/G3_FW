#include "compass_jingles.h"
#include "AudioFileSourcePROGMEM.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

AudioGeneratorWAV *wav_progmem;
AudioFileSourcePROGMEM *file_progmem;
AudioOutputI2S *out_progmem;

void jingle(int id, float gain)
{
    audioLogger = &Serial;
    switch(id)
    {
        case JINGLE_POWER_UP:
            file_progmem = new AudioFileSourcePROGMEM(power_up, sizeof(power_up));
            delay(200); //engine nee
            break;

        case JINGLE_POWER_DOWN:
            file_progmem = new AudioFileSourcePROGMEM(power_down, sizeof(power_down));
            break;
    }
    
    out_progmem = new AudioOutputI2S();
    wav_progmem = new AudioGeneratorWAV();
    wav_progmem->begin(file_progmem, out_progmem);
    out_progmem->SetGain(gain);
    while(1){
        if (wav_progmem->isRunning()){
            if (!wav_progmem->loop()){
            wav_progmem->stop();
            return;
            } 
        }
        ESP.wdtFeed();
    }
}