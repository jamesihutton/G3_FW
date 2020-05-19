/*
	WRITTEN BY JAMES HUTTON 
	2020 - 05 - 15  
*/
#ifndef compass_nonVols_h
#define compass_nonVols_h


const int nonVol_size = 4;

//numerical array index ID of each parameter
enum nonVol_enum{
    MODE,
    TRACK,
    TRACKPOS,
    VOLUME
    //to be added to...
};

//the volitile array of parameters (to be written to SPIFFS, and thus be considered "nonVol")
extern int nonVol[nonVol_size];



int get_nonVols();
int set_nonVols();




#endif