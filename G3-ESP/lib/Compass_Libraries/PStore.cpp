/*
	WRITTEN BY Jesper Noer
    Inspired by the work of James Hutton
	2020 - 08 - 12

    PStore will store user values in Flash to save them across device restarts
*/
#include "PStore.h"

PStore::PStore()
{
    if(LittleFS.begin())
    {
        Serial.println("LittleFS init OK");
        load();
    }
    else
        Serial.println("LittleFS init failed");
}

bool PStore::load()
{
    bool resp = 0;

    //defaults if no file found, or missing  lines
    String s[] =
    {
        DEFAULT_deviceMode,
        DEFAULT_deviceVolume,
        DEFAULT_folderIndex,
        DEFAULT_trackIndex,
        DEFAULT_trackFrame,
        DEFAULT_radioChannel
    };

    //attempt to open file
    File file = LittleFS.open(PSTORE_FILENAME, "r");
    if (!file) {
        resp = 0;
        Serial.println("PStore: Reading from Flash FAILED!!");
    }

    //read values into variables
    else {
        resp = 1;
        Serial.println("PStore: Reading from Flash");

        //read the lines until no longer available or reached expected PSTORE_SIZE
        for (int i = 0; i < PSTORE_SIZE && file.available(); i++){
            s[i] = file.readStringUntil('\n');
            s[i].remove(s[i].lastIndexOf("\n"));
        }
    }

    deviceMode = s[0].toInt(); 
    deviceVolume = s[1].toInt();
    folderIndex = s[2].toInt(); 
    trackIndex = s[3].toInt(); 
    trackFrame = s[4].toInt();
    radioChannel = s[5].toInt();
    
    printValues();

    file.close();
    return resp;
}

bool PStore::save()
{
    File file = LittleFS.open(PSTORE_FILENAME, "w");
    if (!file) {
        Serial.println("PStore: Failed to write to Flash");
        return 0;
    } 
    else {
        //line order is crucial! Order established in class declaration! (PStore.h)
        file.printf("%i\n%i\n%i\n%i\n%i\n%i\n",
            deviceMode, deviceVolume, folderIndex, trackIndex, trackFrame, radioChannel);

        file.close();

        Serial.println("PStore: Writing to Flash");
        printValues();

        return 1;
    }
}

void PStore::resetValues()
{
    Serial.println("PStore: Resetting values");
    deviceMode = (String(DEFAULT_deviceMode)).toInt(); 
    deviceVolume = (String(DEFAULT_deviceVolume)).toInt();
    folderIndex = (String(DEFAULT_folderIndex)).toInt();
    trackIndex = (String(DEFAULT_trackIndex)).toInt();
    trackFrame = (String(DEFAULT_trackFrame)).toInt();
    radioChannel = (String(DEFAULT_radioChannel)).toInt();
    save();
}

void PStore::close()
{
    LittleFS.end();
}

void PStore::printValues()
{
    Serial.printf("%i\n%i\n%i\n%i\n%i\n%i\n\n",
            deviceMode, deviceVolume, folderIndex, trackIndex, trackFrame, radioChannel);
}