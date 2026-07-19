#pragma once
#include <Arduino.h>

void displayInit();
void displayApplyRotation();
void displayBootMessage(const String &line1, const String &line2 = "");
void displaySetupScreen(bool wifiFailed);  // step-by-step onboarding with AP password
void displayRender();       // redraw current provider page
void displayNextPage(int dir);  // slide to next (+1) / previous (-1) provider
int displayPageCount();     // number of enabled providers
void displayStreamBMP(Print &out);  // dump framebuffer as 24-bit BMP (~2-5s)
