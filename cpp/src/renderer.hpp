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
    // Physical rectangle of the last 2D cut plot; empty for the spatial views.
    // Controls use it to map the mouse onto the z = center.z cut plane.
    Rectangle plotRectangle() const { return plotRect_; }
    // Ray-cast a screen point from the last spatial view onto the rendered geometry.
    // Returns false when the last draw was not a spatial view, the pointer is outside
    // the 3D viewport, the ray misses the surface/plane, or the hit is outside the x/y domain.
    bool spatialProbe(Vector2 screen, dipole::Vec3& point) const;
private:
    struct Arrow { dipole::Vec3 point, field; };
    dipole::Parameters p_;
    dipole::Grid grid_;
    std::vector<dipole::Segment> contours_;
    std::vector<Arrow> arrows_;
    std::vector<dipole::Streamline> streamlines_;
    double gridLimit_ = 1, gridPeak_ = 1, fieldMax_ = 1;
    double animationPhase_ = 0;
    bool initialized_ = false;
    Texture2D heatmap_{};
    Mesh surface_{};
    Material material_{};
    RenderTexture2D target_{};
    float yaw_[2] = {.65f, 1.57079632679f}, pitch_[2] = {.55f, 0}, zoom_[2] = {1, 1};
    bool orbiting_[2] = {false, false};
    Rectangle plotRect_{};
    Camera3D spatialCamera_{};
    Rectangle spatialRect_{};
    int spatialWidth_ = 0, spatialHeight_ = 0;
    dipole::Vec3 spatialOrigin_{};
    Vector3 spatialFocusPoint_{};   // scene centre (streamCenter)
    Vector3 spatialFocusNormal_{};  // camera forward at the last draw
    bool spatialSurface_ = false;   // view 3 = height surface
    bool spatialValid_ = false;
    void drawPlane(Rectangle r, int view, const Visibility& v);
    void drawSpatial(Rectangle r, int view, const Visibility& v, bool blocked);
};
}
