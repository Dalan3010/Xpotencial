#pragma once

#include "raylib.h"

namespace app {
    void InitUiFont();     // call once AFTER InitWindow (needs GPU context)
    void UnloadUiFont();   // call once BEFORE CloseWindow
    void DrawTextUi(const char* text, int x, int y, int size, Color color);
    int  MeasureTextUi(const char* text, int size);
}
