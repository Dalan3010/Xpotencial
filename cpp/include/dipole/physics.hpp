#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace dipole {
struct Vec3 { double x = 0, y = 0, z = 0; };
Vec3 operator+(Vec3 a, Vec3 b);
Vec3 operator-(Vec3 a, Vec3 b);
Vec3 operator*(Vec3 a, double s);
double length(Vec3 a);
struct Parameters {
    double q = 1, k = 8.99, d = 2, azimuth = 0, polar = 90;
    int chargeCount = 2; // Source selector: 1 = single charge, 2 = dipole.
    Vec3 center;
    double xmin = -5, xmax = 5, ymin = -5, ymax = 5;
    int resolution = 70, fieldDensity = 14;
    double sphereRadius = 3;
    int sphereResolution = 45;
    double potentialLevel = 4.5, volumeHalfExtent = 3;
    int volumeResolution = 37;
    bool pairedLevels = true;
    double samplingRadius = 2.5, visualRadius = 2, fieldContrast = .45;
    int angularResolution = 40;
};
// Returns an explanation instead of silently accepting invalid numerical data.
std::string validate(const Parameters& p);
struct Charges { Vec3 plus, minus; };
Charges charges(const Parameters& p);
// Hidden length scale: dipole separation, or half the smaller domain span for a single charge.
double referenceLength(const Parameters& p);
double softening(const Parameters& p);
double potential(Vec3 point, const Parameters& p);
Vec3 field(Vec3 point, const Parameters& p);
struct StreamlineOptions {
    int seedCount = 16;
    double seedAngleDegrees = 60; // Angle from the positive dipole axis (dipole seeds only).
    double seedRadius = .08;      // Relative to referenceLength().
    double stepSize = .02;        // Arc-length step relative to referenceLength().
    double endpointRadius = .08;  // Termination radius relative to referenceLength().
    double escapeRadius = 8;      // Escape radius from center relative to referenceLength().
    double maxArcLength = 20;     // Maximum integrated arc length relative to referenceLength().
    int maxSteps = 4000;
};
enum class StreamlineTermination { ReachedNegativeCharge, Escaped, StepLimit, ArcLengthLimit };
struct Streamline {
    std::vector<Vec3> points;
    StreamlineTermination termination = StreamlineTermination::StepLimit;
};
// Integrates deterministic 3D field lines: a ring of seeds about the dipole axis for
// chargeCount == 2, a Fibonacci sphere of seeds for chargeCount == 1.
std::vector<Streamline> streamlines(const Parameters& p, const StreamlineOptions& options = {});
struct Grid {
    int n = 0;
    double xmin = 0, xmax = 0, ymin = 0, ymax = 0;
    std::vector<double> values;
    Vec3 point(int x, int y) const;
    double at(int x, int y) const;
};
Grid makeGrid(const Parameters& p);
struct Segment { Vec3 a, b; double level; };
std::vector<Segment> contours(const Grid& grid, double level);
struct Sample { Vec3 position; double value; };
// Unindexed triangle soup: no 16-bit index overflow at maximum resolution.
using Triangles = std::vector<Sample>;
Triangles sphere(const Parameters& p);
struct AngularSample { Vec3 direction, field; double magnitude = 0; };
struct AngularField {
    int resolution = 0;
    double effectiveRadius = 0, minimum = 0, maximum = 0;
    std::vector<AngularSample> samples;
};
struct RadialSurface {
    std::vector<Sample> vertices; // Dimensionless visual coordinates, not physical meters.
    std::vector<std::array<std::size_t,3>> faces;
    std::vector<std::array<std::size_t,2>> wires; // Only angular meridians/parallels.
};
AngularField sampleFieldAngles(const Parameters& p);
RadialSurface radialSurface(const AngularField& angular, double base, double contrast);
struct Grid3D {
    int n = 0;
    Vec3 minimum, maximum;
    std::vector<double> values;
    Vec3 point(int x, int y, int z) const;
    double at(int x, int y, int z) const;
};
Grid3D makeGrid3D(const Parameters& p);
// Six conforming tetrahedra per cube, linearly interpolated in physical meters.
Triangles equipotentials(const Grid3D& grid, double level, bool paired);
Triangles heightfield(const Grid& grid, double colorLimit, double visualHeight);
double displayLimit(const std::vector<double>& values);
}
