/*
	WRITTEN BY Jesper Noer
    Inspired by the work of James Hutton
	2020 - 08 - 12

    PStore will store user values in Flash to save them across device restarts
*/
#ifndef CPSTORE_H
#define CPSTORE_H

#include <WString.h>
#include <LittleFS.h>

#define DEFAULT_deviceMode      "0"
#define DEFAULT_deviceVolume    "5"
#define DEFAULT_folderIndex     "0"
#define DEFAULT_trackIndex      "0"
#define DEFAULT_trackFrame      "0"
#define DEFAULT_radioChannel    "9470"

#define PSTORE_FILENAME         "/pstore.txt"
#define PSTORE_SIZE             6

class PStore
{
    private:
        void printValues();

    public:
        int deviceMode;             //mode of the device (mp3 = 0, radio = 1)
        int deviceVolume;           //current volume
        int folderIndex;            //Index of the current folder
        int trackIndex;             //Index of the current track in the folder
        int trackFrame;             //frame of current track (position)
        int radioChannel;           //current radio frequency

        PStore();
        bool load();
        bool save();
        void resetValues();
        void close();
};
typedef PStore PStoreClass;

#endif