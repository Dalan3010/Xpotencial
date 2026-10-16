#include "ui_text.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

namespace app {
namespace {
struct FontEntry {
    int size;
    Font font;
    bool isDefaultFallback;
};

constexpr std::array<int, 9> kFontSizes = {{12, 13, 14, 15, 16, 17, 18, 21, 22}};

// Codepoints for Latin-1/Spanish coverage (32..255)
static std::vector<int> BuildCodepoints() {
    std::vector<int> cp(224);
    for (int i = 0; i < 224; ++i) {
        cp[i] = 32 + i;
    }
    return cp;
}

static const std::vector<int>& GetCodepoints() {
    static std::vector<int> cp = BuildCodepoints();
    return cp;
}

static std::array<FontEntry, 9> gFontEntries = []() {
    std::array<FontEntry, 9> entries;
    for (size_t i = 0; i < kFontSizes.size(); ++i) {
        entries[i] = {kFontSizes[i], GetFontDefault(), true};
    }
    return entries;
}();

static bool gInitialized = false;

const FontEntry* FindBestEntry(int size) {
    const FontEntry* best = nullptr;
    for (const auto& entry : gFontEntries) {
        if (entry.font.texture.id == 0) {
            continue;
        }
        if (entry.size == size) {
            return &entry;
        }
        if (entry.size <= size) {
            if (best == nullptr || entry.size > best->size) {
                best = &entry;
            }
        }
    }
    if (best != nullptr) {
        return best;
    }
    // Fallback to smallest available (could be default)
    for (const auto& entry : gFontEntries) {
        if (entry.font.texture.id != 0) {
            return &entry;
        }
    }
    return &gFontEntries[0];
}

static void UnloadEntry(FontEntry& entry) {
    if (entry.isDefaultFallback) {
        return;
    }
    if (entry.font.texture.id != 0) {
        UnloadFont(entry.font);
        entry.font = GetFontDefault();
        entry.isDefaultFallback = true;
    }
}

static std::string GetFontPath() {
    const char* appDir = GetApplicationDirectory();
    if (appDir == nullptr || appDir[0] == '\0') {
        return std::string("IBMPlexSans-Regular.ttf");
    }
    std::string path(appDir);
    // Ensure trailing separator if needed
    if (path.size() > 0 && path.back() != '\\' && path.back() != '/') {
#if defined(_WIN32)
        path += '\\';
#else
        path += '/';
#endif
    }
    path += "IBMPlexSans-Regular.ttf";
    return path;
}

static bool WarnedDefaultOnce = false;
} // namespace

void InitUiFont() {
    if (gInitialized) {
        return;
    }
    const auto& cp = GetCodepoints();
    const int cpCount = static_cast<int>(cp.size());
    const std::string fontPath = GetFontPath();
    for (size_t i = 0; i < kFontSizes.size(); ++i) {
        const int sz = kFontSizes[i];
        // Try to load from fontPath
        Font loaded = LoadFontEx(fontPath.c_str(), sz, const_cast<int*>(cp.data()), cpCount);
        if (loaded.texture.id == 0) {
            if (!WarnedDefaultOnce) {
                std::cerr << "WARNING: Failed to load UI font " << fontPath << "; falling back to default font for size " << sz << "\n";
                WarnedDefaultOnce = true;
            }
            gFontEntries[i] = {sz, GetFontDefault(), true};
        } else {
            gFontEntries[i] = {sz, loaded, false};
        }
    }
    gInitialized = true;
}

void UnloadUiFont() {
    for (auto& entry : gFontEntries) {
        UnloadEntry(entry);
    }
    gInitialized = false;
}

void DrawTextUi(const char* text, int x, int y, int size, Color color) {
    if (text == nullptr) {
        return;
    }
    const FontEntry* entry = FindBestEntry(size);
    if (entry == nullptr) {
        DrawText(text, x, y, size, color);
        return;
    }
    const float spacing = static_cast<float>(size) / 10.0f;
    DrawTextEx(entry->font, text, Vector2{static_cast<float>(x), static_cast<float>(y)}, static_cast<float>(size), spacing, color);
}

int MeasureTextUi(const char* text, int size) {
    if (text == nullptr || text[0] == '\0') {
        return 0;
    }
    const FontEntry* entry = FindBestEntry(size);
    if (entry == nullptr) {
        return MeasureText(text, size);
    }
    const float spacing = static_cast<float>(size) / 10.0f;
    const Vector2 dims = MeasureTextEx(entry->font, text, static_cast<float>(size), spacing);
    return static_cast<int>(dims.x);
}
} // namespace app
