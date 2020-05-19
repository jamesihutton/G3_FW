

#include "compass_nonVols.h"
#include "FS.h"
#include <string.h>


int nonVol[nonVol_size] = {1,2,3,4};

//gets the nonVols from SPIFFS and stores them in nonVol[]
int get_nonVols()
{
    //stream stuff: https://www.arduino.cc/reference/en/language/functions/communication/stream/
    File f = SPIFFS.open("/f.txt", "r");
    if (!f) {
        Serial.println("\nspiffs file failed to open");
        return (0);
    } 
    else {
        Serial.println("\n\nReading from spiffs: \n\n");
        String s;
        for(int i = 0; i<nonVol_size || f.available(); i++) {
            s = f.readStringUntil('\n');
            s.remove(s.lastIndexOf("\n"));
            Serial.println(s);

            if (i==TRACKPOS) {
                nonVol[TRACKPOS] = s.toInt();
            }

        }
        Serial.println("\n\nfinished...\n\n");
        return (1);
    }
}

//writes nonVol[] to SPIFFS
int set_nonVols()
{

    File f = SPIFFS.open("/f.txt", "w");
    if (!f) {
        Serial.println("\nspiffs file failed to open");
        return (0);
    } 
    else {

        Serial.println("\n\nWriting to SPIFFS:\n\n");
        for (int i = 0; i<nonVol_size; i++){
           f.println(nonVol[i]);
           Serial.println(nonVol[i]);           
        }
        f.close();
        Serial.println("done...");
        return(1);
    }
    
}








