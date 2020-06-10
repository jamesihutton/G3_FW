/*
	WRITTEN BY JAMES HUTTON 
	2020 - 05 - 15  
*/
#ifndef compass_nonVols_h
#define compass_nonVols_h


#include <WString.h>

#define DEFAULT_deviceMode      "0"
#define DEFAULT_deviceVolume    "5"
#define DEFAULT_folderIndex     "0"
#define DEFAULT_trackIndex      "0"
#define DEFAULT_trackFrame      "0"
#define DEFAULT_radioChannel    "9470"

const int nonVol_size = 6;

class nonVol
{
    private:

    public:
        int deviceMode;             //mode of the device (mp3 = 0, radio = 1)
        int deviceVolume;           //current volume
        int folderIndex;            //Index of the current folder
        int trackIndex;             //Index of the current track in the folder
        int trackFrame;             //frame of current track (position)
        int radioChannel;           //frame of current track (position)
        
        /**********************************************************
         * when adding parameters, ensure you also add them to 
         * "get_nonVols" & "set_nonVols" 
         * IN THE SAME LINE ORDER!
         **********************************************************/





        int set_nonVols();    //sets the flash to current device params (usually for powerdown)
        int get_nonVols();    //retrieves saved params from flash to resume playback (usually for startup)

};
typedef nonVol nonVolClass;


int SPIFFS_line_to_int(String s);

String SPIFFS_line_to_string(String s);


#endif