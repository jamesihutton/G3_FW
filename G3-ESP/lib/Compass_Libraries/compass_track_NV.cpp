#include "compass_nonVols.h"
#include "LittleFS.h"
#include <WString.h>



int track_nv_getFrame(int folder, int track)
{
    int resp;
    String fileName = String("/track_nv:" + String(folder) + ":" + String(track) + ".txt");
    int frame;
    Serial.print("FileName: "); Serial.println(fileName);
    File f = LittleFS.open(fileName, "r");
    if (!f) {
        Serial.println("\ntrack_nv file failed to open for reading");
        resp = -1;
    } 
    else {
        String s;
        if(f.available()){
            s = f.readStringUntil('\n');  //read next line
            Serial.print("\nRead line: "); Serial.println(s);
            s.remove(s.lastIndexOf("\n")); //remove the \n
            frame = s.toInt();
            Serial.printf("Resuming from saved frame: %i\n", frame);
            resp = frame;
        } else resp = -1;
    }

    f.close();
    return resp;
}

int track_nv_setFrame(int folder, int track, int frame)
{
    String fileName = String("/track_nv:" + String(folder) + ":" + String(track) + ".txt");
    File f = LittleFS.open(fileName, "w");
    if (!f) {
        Serial.println("\ntrack_nv file failed to open for writing");
        return (0);
    } 
    Serial.printf("\nWriting %i frame to file: ", frame); Serial.println(fileName);
    f.println(frame);
    f.close();
    return 1;
}