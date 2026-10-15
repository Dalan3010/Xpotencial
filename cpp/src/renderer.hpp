#pragma once
#include "controls.hpp"
#include "raylib.h"
#include <vector>

namespace app {
class Renderer {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    void update(const dipole::Parameters& p);
    void draw(Rectangle viewport, int view, const Visibility& visibility, bool cameraBlocked);
private:
    struct Arrow { dipole::Vec3 point, field; };
    dipole::Parameters p_;
    dipole::Grid grid_;
    std::vector<dipole::Segment> contours_;
    std::vector<Arrow> arrows_;
    std::vector<dipole::Streamline> streamlines_;
    double gridLimit_ = 1, fieldMax_ = 1;
    double animationPhase_ = 0;
    bool initialized_ = false;
    Texture2D heatmap_{};
    Mesh surface_{};
    Material material_{};
    RenderTexture2D target_{};
    float yaw_[2] = {.65f, 1.57079632679f}, pitch_[2] = {.55f, 0}, zoom_[2] = {1, 1};
    bool orbiting_[2] = {false, false};
    void drawPlane(Rectangle r, int view, const Visibility& v);
    void drawSpatial(Rectangle r, int view, const Visibility& v, bool blocked);
};
}
