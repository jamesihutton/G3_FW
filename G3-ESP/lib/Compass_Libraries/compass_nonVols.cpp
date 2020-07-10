

#include "compass_nonVols.h"
#include "LittleFS.h"
#include <WString.h>



//retrieves saved params from flash to resume playback (usually for startup)
int nonVol::get_nonVols()
{
    //stream stuff: https://www.arduino.cc/reference/en/language/functions/communication/stream/
    int resp = 0;

    //defaults if no file found, or missing  lines
    String s[nonVol_size];
    s[0] = DEFAULT_deviceMode;
    s[1] = DEFAULT_deviceVolume;
    s[2] = DEFAULT_folderIndex;
    s[3] = DEFAULT_trackIndex;
    s[4] = DEFAULT_trackFrame;
    s[5] = DEFAULT_radioChannel;
    
    //attempt to open SPIFFS file
    File f = LittleFS.open("/nv.txt", "r");
    if (!f) {
        resp = 0;
        Serial.println("\nspiffs file failed to open");
        
    } 

    //read file into nonVols
    else {
        resp = 1;
        Serial.println("\n\nReading from SPIFFS: \n\n");
                
        int lineCount;
        //read the lines until no longer available or reached expected nonVol_size
        for (lineCount = 0; lineCount<nonVol_size && f.available(); lineCount++){
            s[lineCount] = f.readStringUntil('\n');
            s[lineCount].remove(s[lineCount].lastIndexOf("\n"));
        }
    }

    nonVol::deviceMode = SPIFFS_line_to_int(s[0]); 
    nonVol::deviceVolume = SPIFFS_line_to_int(s[1]);
    nonVol::folderIndex = SPIFFS_line_to_int(s[2]); 
    nonVol::trackIndex = SPIFFS_line_to_int(s[3]); 
    nonVol::trackFrame = SPIFFS_line_to_int(s[4]);
    nonVol::radioChannel = SPIFFS_line_to_int(s[5]);
    
    Serial.println(nonVol::deviceMode);
    Serial.println(nonVol::deviceVolume);
    Serial.println(nonVol::folderIndex);
    Serial.println(nonVol::trackIndex);
    Serial.println(nonVol::trackFrame);
    Serial.println(nonVol::radioChannel);

    f.close();
    return (resp);
    
}

//sets the flash to current device params (usually for powerdown)
int nonVol::set_nonVols()
{

    File f = LittleFS.open("/nv.txt", "w");
    if (!f) {
        Serial.println("\nspiffs file failed to open");
        return (0);
    } 
    else {

        Serial.println("\n\nWriting to SPIFFS:\n\n");

        //line order is crucial! Order established in class declaration! (compass_nonVols.h)
        f.println(nonVol::deviceMode);
        f.println(nonVol::deviceVolume);
        f.println(nonVol::folderIndex);
        f.println(nonVol::trackIndex);
        f.println(nonVol::trackFrame);
        f.println(nonVol::radioChannel);
       
        Serial.println(nonVol::deviceMode);
        Serial.println(nonVol::deviceVolume);
        Serial.println(nonVol::folderIndex);
        Serial.println(nonVol::trackIndex);
        Serial.println(nonVol::trackFrame);
        Serial.println(nonVol::radioChannel);


        f.close();
        Serial.println("done... ");
        return(1);
    }
    
}

int SPIFFS_line_to_int(String s)
{
    return s.toInt();
}








