#pragma once
#include "dipole/physics.hpp"
#include "raylib.h"
#include <string>

namespace app {
struct Visibility {
    bool charges = true, labels = true, contours = true, grid = true, field = false;
    bool animationMarkers = true, animationPaused = false;
    float animationSpeed = 1.0f;
};
class Controls {
public:
    dipole::Parameters parameters;
    Visibility visible;
    int view = 0;
    bool help = false;
    bool draw(Rectangle plotRect = {}); // True only after a valid physical parameter change.
    Rectangle viewport() const;
    bool blocksCamera() const { return help || editing_ >= 0 || dragging_ >= 0; }
private:
    float scroll_ = 0;
    int editing_ = -1, dragging_ = -1;
    bool selectAll_ = false;
    std::string buffer_, error_;
    dipole::Vec3 probe_{};   // Last physical point sampled on the cut plane.
    bool hasProbe_ = false;
    bool apply(int index, double value);
    bool parameterRow(int index, float y);
    void probePoint(const Rectangle& plotRect);
    void drawHelp(const Rectangle& plotRect);
};
}
