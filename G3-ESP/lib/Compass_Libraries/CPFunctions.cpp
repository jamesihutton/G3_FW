#include "CPFunctions.h"

void mute_amp() {
  digitalWrite(MUTE_PIN, MUTE);
}

void unmute_amp() {
  digitalWrite(MUTE_PIN, UNMUTE);
}
