#pragma once
#include <Arduino.h>

void webInit();
void webLoop();
bool webConfigChanged();  // true once after a config save (consume-on-read)
