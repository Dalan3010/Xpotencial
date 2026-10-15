#include "dipole/physics.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <limits>
#include <map>
#include <array>
#include <stdexcept>
using namespace dipole;
namespace {
int checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
bool near(double a, double b, double tolerance = 1e-8) {
    return std::abs(a-b) <= tolerance;
}
void physicsTests() {
    Parameters p;
    auto c = charges(p);
    check(near(c.plus.x, 1) && near(c.minus.x, -1), "default charge geometry");
    check(near(length(c.plus-c.minus), 2), "separation");
    check(near(potential({0,2,0}, p), 0), "bisector symmetry");
    const double v = potential({2,0,0}, p);
    check(near(v, 8.99*(1/std::sqrt(1.000004)-1/std::sqrt(9.000004))), "nC / k convention");
    check(near(potential({-2,0,0}, p), -v), "odd symmetry");
    p.q = 2;
    check(near(potential({2,0,0}, p), 2*v), "charge scaling");
    p.k *= 2;
    check(near(potential({2,0,0}, p), 4*v), "Coulomb scaling");
    p = Parameters{}; p.azimuth = 90;
    check(near(charges(p).plus.y, 1), "azimuth orientation");
    p.polar = 0;
    check(near(charges(p).plus.z, 1), "polar orientation");
    p = Parameters{}; p.center = {3,-2,4};
    check(near(potential({5,-2,4}, p), v), "translation invariance");
    check(std::isfinite(potential(charges(p).plus,p)), "finite softened singularity");
    check(near(softening(p), .002), "adaptive softening");
    auto e = field({5,-2,4},p);
    check(e.x > 0 && std::abs(e.y) < 1e-7 && std::abs(e.z) < 1e-7, "field direction");
    const double eps = softening(p);
    const double exact = 8.99*(1/std::pow(1+eps*eps,1.5)-3/std::pow(9+eps*eps,1.5));
    check(near(e.x, exact, 1e-5), "centered derivative accuracy");
    p = Parameters{};
    check(validate(p).empty(), "defaults valid");
    p.xmin = p.xmax;
    check(!validate(p).empty(), "reject empty domain");
    p = Parameters{}; p.q = std::numeric_limits<double>::quiet_NaN();
    check(!validate(p).empty(), "reject NaN");
    p = Parameters{}; p.center.z = std::numeric_limits<double>::infinity();
    check(!validate(p).empty(), "reject infinite center");
    p = Parameters{}; p.resolution = 151;
    check(!validate(p).empty(), "bound grid allocation");
    p = Parameters{}; p.sphereResolution = 101;
    check(!validate(p).empty(), "bound sphere allocation");
    bool rejected = false;
    try { makeGrid(p); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "builders reject invalid input");
    p = Parameters{}; p.d = 0;
    check(!validate(p).empty(), "reject zero separation");
    p = Parameters{}; p.fieldDensity = 31;
    check(!validate(p).empty(), "bound field allocation");
    p = Parameters{}; p.sphereRadius = -.5;
    check(!validate(p).empty(), "reject negative sphere radius");
    p = Parameters{}; p.xmax = 20000;
    check(!validate(p).empty(), "bound world coordinates for GPU precision");
    p = Parameters{}; p.polar = 181;
    check(!validate(p).empty(), "bound orientation");
    p = Parameters{}; p.resolution = 15; p.sphereResolution = 10; p.fieldDensity = 4;
    check(validate(p).empty(), "minimum resolution limits valid");
    p.resolution = 150; p.sphereResolution = 100; p.fieldDensity = 30;
    check(validate(p).empty(), "maximum resolution limits valid");
}
Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
double dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
void radialTests() {
    Parameters p;
    auto samples=sampleFieldAngles(p);
    auto mesh=radialSurface(samples,p.visualRadius,p.fieldContrast);
    check(near(samples.effectiveRadius,2.5), "default physical probe distance");
    bool above=false, below=false, nonNormal=false;
    for (size_t i=0;i<mesh.vertices.size();++i) {
        const auto& a=samples.samples[i];
        const double magnitude=length(field(p.center+a.direction*samples.effectiveRadius,p));
        check(near(a.magnitude,magnitude,1e-7), "actual magnitude at common reference distance");
        const double s=(magnitude-samples.minimum)/(samples.maximum-samples.minimum);
        const double radius=length(mesh.vertices[i].position);
        check(near(radius,p.visualRadius*(1+p.fieldContrast*(2*s-1)),1e-7), "magnitude associated with actual displayed radius");
        check(std::isfinite(radius) && radius>=1.1-1e-8 && radius<=2.9+1e-8, "finite positive bounded radius");
        above |= radius>p.visualRadius; below |= radius<p.visualRadius;
    }
    check(above && below, "bulges and inward depressions");
    auto baseline=radialSurface(samples,2,0);
    for (const auto& v:baseline.vertices) check(near(length(v.position),2,1e-12), "alpha zero baseline diagnostic");
    auto uniform=samples;
    for (auto& s:uniform.samples) s.magnitude=3;
    uniform.minimum=uniform.maximum=3;
    for (const auto& v:radialSurface(uniform,2,.9).vertices)
        check(near(length(v.position),2,1e-12), "zero contrast falls back to baseline");
    uniform.samples[0].magnitude=3+1e-10; uniform.maximum=3+1e-10;
    for (const auto& v:radialSurface(uniform,2,.9).vertices)
        check(near(length(v.position),2,1e-12), "negligible numerical contrast does not invent a shape");
    uniform.samples[0].magnitude=std::numeric_limits<double>::infinity();
    bool rejected=false;
    try { radialSurface(uniform,2,.4); } catch (const std::invalid_argument&) { rejected=true; }
    check(rejected,"reject nonfinite angular samples");
    for (int n:{10,40,80}) {
        p.angularResolution=n;
        auto angular=sampleFieldAngles(p);
        auto m=radialSurface(angular,2,.9);
        std::map<std::pair<size_t,size_t>,int> edges;
        for (const auto& face:m.faces) {
            auto a=m.vertices[face[0]].position,b=m.vertices[face[1]].position,c=m.vertices[face[2]].position;
            auto normal=cross(b-a,c-a);
            check(length(normal)>1e-12 && dot(normal,a)>0,"outward nondegenerate radial winding");
            auto e=angular.samples[face[0]].field;
            nonNormal |= std::abs(dot(normal,e))/(length(normal)*length(e))<.8;
            for (int j=0;j<3;++j) {
                size_t x=face[j],y=face[(j+1)%3]; if (y<x) std::swap(x,y);
                ++edges[{x,y}];
            }
        }
        for (auto edge:edges) check(edge.second==2,"shared seam/poles two-face incidence");
        check(m.vertices.size()==static_cast<size_t>(2+(n-1)*2*n),"exact shared poles and periodic lattice");
    }
    check(nonNormal,"radial surface normal is not generally parallel to E");
    p=Parameters{};
    auto original=sampleFieldAngles(p);
    auto originalMesh=radialSurface(original,2,.45);
    for (int mode=0;mode<4;++mode) {
        auto changed=p;
        if (mode==0) changed.q*=2;
        if (mode==1) changed.k*=2;
        if (mode==2) changed.center={3,-2,4};
        if (mode==3) changed.azimuth=90;
        auto alt=sampleFieldAngles(changed);
        auto altMesh=radialSurface(alt,2,.45);
        const int slices=2*p.angularResolution;
        for (size_t i=0;i<original.samples.size();++i) {
            size_t j=i;
            if (mode==3 && i>0 && i+1<original.samples.size()) {
                size_t ring=(i-1)/slices, f=(i-1)%slices;
                j=1+ring*slices+(f+slices/4)%slices;
            }
            check(near(alt.samples[j].magnitude,original.samples[i].magnitude*(mode<2 ? 2 : 1),2e-6),"q/k scaling or rotated/translated sample invariance");
            check(near(length(altMesh.vertices[j].position),length(originalMesh.vertices[i].position),2e-6),"normalized radii invariant under scaling/rigid motion");
        }
    }
    auto polar=p; polar.polar=0; polar.center={-3,2,1};
    auto polarSamples=sampleFieldAngles(polar);
    auto polarMesh=radialSurface(polarSamples,2,.45);
    const int n=p.angularResolution,slices=2*n;
    auto index=[&](int t,int f) { return static_cast<size_t>(1+(t-1)*slices+(f+slices)%slices); };
    check(length(originalMesh.vertices[index(n/2,0)].position)>length(originalMesh.vertices[0].position),"axial bulge versus equatorial depression");
    check(near(length(polarMesh.vertices[0].position),length(originalMesh.vertices[index(n/2,0)].position),2e-6),"polar rotation moves axial bulge to pole");
    for (size_t i=0;i<polarSamples.samples.size();++i) {
        auto u=polarSamples.samples[i].direction;
        // Inverse quarter turn maps the polar z-axis back to the default x-axis.
        const double expected=length(field(p.center+Vec3{u.z,u.y,-u.x}*original.effectiveRadius,p));
        check(near(expected,polarSamples.samples[i].magnitude,2e-6),"polar rotated and translated field samples");
    }
    for (int t=1;t<n;++t) for (int f=0;f<slices;++f) {
        double v=original.samples[index(t,f)].magnitude;
        check(near(v,original.samples[index(n-t,f)].magnitude,2e-6),"z reflection symmetry");
        check(near(v,original.samples[index(t,slices/2-f)].magnitude,2e-6),"dipole axial reflection symmetry");
    }
    // Equatorial directions around the dipole x-axis share the same magnitude.
    check(near(original.samples[0].magnitude,original.samples[index(n/2,slices/4)].magnitude,2e-6),"axial rotational symmetry");
    p.d=6; p.samplingRadius=.5;
    check(near(sampleFieldAngles(p).effectiveRadius,4.5),"effective probe safety adjustment");
    for (int mode=0;mode<10;++mode) {
        p=Parameters{};
        if (mode==0) p.samplingRadius=.49;
        if (mode==1) p.samplingRadius=20.01;
        if (mode==2) p.visualRadius=0;
        if (mode==3) p.visualRadius=10.01;
        if (mode==4) p.fieldContrast=-.01;
        if (mode==5) p.fieldContrast=.91;
        if (mode==6) p.angularResolution=9;
        if (mode==7) p.angularResolution=81;
        if (mode==8) p.samplingRadius=std::numeric_limits<double>::quiet_NaN();
        if (mode==9) p.fieldContrast=std::numeric_limits<double>::infinity();
        check(!validate(p).empty(),"invalid radial parameters rejected");
    }
    p=Parameters{}; p.samplingRadius=20; p.visualRadius=.1; p.fieldContrast=.9; p.angularResolution=80;
    check(validate(p).empty(),"radial boundary parameters valid");
}
void streamlineTests() {
    Parameters p;
    StreamlineOptions options;
    options.seedCount = 12;
    const auto paths = streamlines(p, options);
    check(paths.size() == static_cast<size_t>(options.seedCount), "one deterministic streamline per seed");
    for (const auto& path : paths) {
        check(path.termination == StreamlineTermination::ReachedNegativeCharge, "default dipole paths terminate at negative charge");
        check(path.points.size() > 10 && path.points.size() <= static_cast<size_t>(options.maxSteps + 1), "streamline length is bounded");
        check(near(length(path.points.front() - charges(p).plus), options.seedRadius*p.d, 1e-8), "streamline begins on positive-charge seed shell");
        check(length(path.points.back() - charges(p).minus) <= options.endpointRadius*p.d, "streamline reaches negative-charge endpoint region");
        for (size_t i = 0; i < path.points.size(); ++i) {
            const auto point = path.points[i];
            check(std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z), "streamline points remain finite");
            if (i > 0 && i + 1 < path.points.size()) {
                const Vec3 tangent = path.points[i + 1] - path.points[i - 1];
                const Vec3 e = field(point, p);
                check(dot(tangent, e) / (length(tangent) * length(e)) > .95, "streamline tangent follows electric field");
            }
        }
    }
    for (size_t i = 0; i < paths[0].points.size(); ++i) {
        check(i < paths[options.seedCount / 2].points.size(), "opposite symmetric streamline has corresponding samples");
        const auto a = paths[0].points[i], b = paths[options.seedCount / 2].points[i];
        check(near(a.x, b.x, 2e-5) && near(a.y, -b.y, 2e-5) && near(a.z, b.z, 2e-5), "upper and lower streamlines are mirror symmetric");
    }

    Parameters moved = p;
    moved.center = {3, -2, 4};
    moved.azimuth = 90;
    const auto transformed = streamlines(moved, options);
    check(transformed.size() == paths.size(), "translated and rotated dipole preserves seed count");
    for (size_t i = 0; i < paths.size(); ++i) {
        check(paths[i].points.size() == transformed[i].points.size(), "rigidly transformed streamline preserves integration length");
        for (size_t j = 0; j < paths[i].points.size(); ++j) {
            const auto a = paths[i].points[j], b = transformed[i].points[j];
            check(near(b.x, 3 - a.y, 2e-5) && near(b.y, -2 + a.x, 2e-5) && near(b.z, 4 + a.z, 2e-5), "streamline follows translated and rotated physical coordinates");
        }
    }

    StreamlineOptions limited = options;
    limited.maxSteps = 1;
    const auto truncated = streamlines(p, limited);
    check(std::all_of(truncated.begin(), truncated.end(), [](const Streamline& path) {
        return path.termination == StreamlineTermination::StepLimit && path.points.size() <= 2;
    }), "integration stops at configured step bound");
    limited = options;
    limited.maxArcLength = .1;
    const auto arcLimited = streamlines(p, limited);
    check(std::all_of(arcLimited.begin(), arcLimited.end(), [](const Streamline& path) {
        return path.termination == StreamlineTermination::ArcLengthLimit && path.points.size() > 1;
    }), "integration stops at configured arc-length bound");

    Parameters oriented = p;
    oriented.d = 4;
    oriented.azimuth = 180;
    oriented.polar = 40;
    oriented.center = {-5, 3, 2};
    StreamlineOptions alternate = options;
    alternate.seedCount = 8;
    alternate.seedAngleDegrees = 45;
    for (const auto& path : streamlines(oriented, alternate)) {
        check(path.termination == StreamlineTermination::ReachedNegativeCharge, "alternate 3D orientation still connects opposite charges");
        for (const auto point : path.points)
            check(std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z), "alternate physical layout remains finite");
    }

    limited = options;
    limited.escapeRadius = 1.01;
    const auto escaped = streamlines(p, limited);
    check(std::all_of(escaped.begin(), escaped.end(), [](const Streamline& path) {
        return path.termination == StreamlineTermination::Escaped;
    }), "integration safely reports paths beyond escape bound");

    for (int invalid = 0; invalid < 4; ++invalid) {
        auto bad = options;
        if (invalid == 0) bad.seedCount = 0;
        if (invalid == 1) bad.maxSteps = 0;
        if (invalid == 2) bad.stepSize = std::numeric_limits<double>::quiet_NaN();
        if (invalid == 3) bad.seedCount = 1000000;
        bool rejected = false;
        try { streamlines(p, bad); } catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "invalid or unbounded streamline options are rejected");
    }
    p.q = std::numeric_limits<double>::infinity();
    bool rejected = false;
    try { streamlines(p); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "invalid physical parameters are rejected for streamline integration");
}
void singleChargeTests() {
    Parameters p;
    p.chargeCount = 1;
    check(validate(p).empty(), "single-charge defaults valid");
    auto c = charges(p);
    check(near(c.plus.x,p.center.x) && near(c.plus.y,p.center.y) && near(c.plus.z,p.center.z),
          "single charge sits at the center");
    check(near(c.minus.x,p.center.x) && near(c.minus.y,p.center.y) && near(c.minus.z,p.center.z),
          "unused partner marker sits at the center");
    check(near(referenceLength(p),5), "single-charge reference length is half the smaller span");
    check(near(softening(p),.005), "single-charge softening uses the reference length");

    // Acceptance: V(center + r u) == k q / r and E is radial with magnitude k q / r^2.
    const Vec3 u{0,0,1};
    const double r = 3;
    const Vec3 probe = p.center + u*r;
    check(near(potential(probe,p),p.k*p.q/r,1e-5), "single-charge potential k q / r");
    auto e = field(probe,p);
    check(near(length(e),p.k*p.q/(r*r),1e-4), "single-charge field magnitude k q / r^2");
    check(e.z>0 && std::abs(e.x)<1e-9 && std::abs(e.y)<1e-9, "single-charge field points radially outward");
    auto rotated = p; rotated.center = {3,-2,4};
    auto er = field(rotated.center + Vec3{0,2,0},rotated);
    check(er.y>0 && std::abs(er.x)<1e-9 && std::abs(er.z)<1e-9, "single-charge field follows a translated center");

    // A lone positive charge has no bisector: every grid value is strictly positive.
    p.resolution = 15;
    auto g = makeGrid(p);
    check(g.values.size() == 225, "single-charge grid count");
    check(std::all_of(g.values.begin(),g.values.end(),[](double v){ return v>0; }),
          "single-charge grid is strictly positive");
    check(contours(g,0).empty(), "single-charge map has no zero equipotential");

    // Validation: only 1 and 2 are accepted as source counts.
    p = Parameters{}; p.chargeCount = 0;
    check(!validate(p).empty(), "reject zero charge count");
    p.chargeCount = 3;
    check(!validate(p).empty(), "reject three charges");
    p.chargeCount = -1;
    check(!validate(p).empty(), "reject negative charge count");
    p.chargeCount = 2;
    check(validate(p).empty(), "explicit dipole charge count valid");

    // Dipole regression: historical values are unchanged when chargeCount == 2.
    Parameters reg;
    check(reg.chargeCount == 2, "dipole remains the default source");
    check(near(referenceLength(reg),reg.d), "dipole reference length equals separation");
    check(near(softening(reg),.002), "dipole regression adaptive softening");
    check(near(potential({2,0,0},reg),8.99*(1/std::sqrt(1.000004)-1/std::sqrt(9.000004))),
          "dipole regression potential value");
    auto rc = charges(reg);
    check(near(rc.plus.x,1) && near(rc.minus.x,-1), "dipole regression charge geometry");
    check(near(length(rc.plus-rc.minus),2), "dipole regression separation");
    {
        // Point at distance 1 from the positive charge and 3 from the negative one.
        const double eps = softening(reg);
        const double exact = 8.99*(1/std::pow(1+eps*eps,1.5)-3/std::pow(9+eps*eps,1.5));
        check(near(field({2,0,0},reg).x,exact,1e-5), "dipole regression field value");
    }
}
void monopoleStreamlineTests() {
    Parameters p;
    p.chargeCount = 1;
    StreamlineOptions options;
    options.seedCount = 12;
    const double scale = referenceLength(p);
    const auto paths = streamlines(p, options);
    check(paths.size() == static_cast<size_t>(options.seedCount), "one monopole streamline per seed");
    for (const auto& path : paths) {
        check(path.termination != StreamlineTermination::ReachedNegativeCharge,
              "monopole paths never sink into a charge");
        check(path.termination == StreamlineTermination::Escaped,
              "monopole paths terminate by escape radius");
        check(path.points.size() > 10 && path.points.size() <= static_cast<size_t>(options.maxSteps+1),
              "monopole streamline length is bounded");
        check(near(length(path.points.front() - p.center), options.seedRadius*scale, 1e-8),
              "monopole seeds start on the reference sphere");
        for (size_t i = 0; i < path.points.size(); ++i) {
            const auto point = path.points[i];
            check(std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z),
                  "monopole points remain finite");
            if (i > 0)
                check(length(point-p.center) > length(path.points[i-1]-p.center),
                      "monopole paths move strictly away from the charge");
        }
    }

    Parameters moved = p;
    moved.center = {3,-2,4};
    const auto transformed = streamlines(moved, options);
    check(transformed.size() == paths.size(), "translated single charge preserves seed count");
    for (const auto& path : transformed) {
        check(path.termination == StreamlineTermination::Escaped, "translated single charge still escapes");
        check(near(length(path.points.front()-moved.center), options.seedRadius*scale, 1e-8),
              "monopole seeds follow the translated center");
    }

    StreamlineOptions limited = options;
    limited.maxSteps = 1;
    const auto truncated = streamlines(p, limited);
    check(std::all_of(truncated.begin(), truncated.end(), [](const Streamline& path) {
        return path.termination == StreamlineTermination::StepLimit && path.points.size() <= 2;
    }), "monopole integration stops at configured step bound");
    limited = options;
    limited.maxArcLength = .1;
    const auto arcLimited = streamlines(p, limited);
    check(std::all_of(arcLimited.begin(), arcLimited.end(), [](const Streamline& path) {
        return path.termination == StreamlineTermination::ArcLengthLimit && path.points.size() > 1;
    }), "monopole integration stops at configured arc-length bound");

    Parameters bad = p;
    bad.chargeCount = 0;
    bool rejected = false;
    try { streamlines(bad, options); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "invalid charge count is rejected for streamline integration");
}
void affinePlaneTopologyTests() {
    struct Case {
        const char* name;
        Vec3 normal;
        double level, area, perimeter;
    };
    // Sections of [-1,1]^3: a diagonal rectangle or a (possibly irregular) hexagon.
    // For |level| <= 1, x+y+z=level has area sqrt(3)*(3-level^2).
    const Case cases[] = {
        {"x+y=0", {1,1,0}, 0, 4*std::sqrt(2.0), 4+4*std::sqrt(2.0)},
        {"x+y=0.5", {1,1,0}, .5, 3*std::sqrt(2.0), 4+3*std::sqrt(2.0)},
        {"x+y+z=0", {1,1,1}, 0, 3*std::sqrt(3.0), 6*std::sqrt(2.0)},
        {"x+y+z=0.5", {1,1,1}, .5, 2.75*std::sqrt(3.0), 6*std::sqrt(2.0)}
    };
    using Key = std::array<double,3>;
    using Edge = std::pair<Key,Key>;
    auto key = [](Vec3 a) { return Key{a.x,a.y,a.z}; };
    for (const auto& test : cases) for (int n : {5,9}) {
        const std::string context = std::string(test.name)+" grid "+std::to_string(n)+": ";
        auto verify = [&](bool condition, const char* message) {
            check(condition,(context+message).c_str());
        };
        Grid3D g{n,{-1,-1,-1},{1,1,1},{}};
        int exactNodes = 0, exactGridEdges = 0;
        for (int z=0;z<n;++z) for (int y=0;y<n;++y) for (int x=0;x<n;++x) {
            const double value = dot(test.normal,g.point(x,y,z));
            g.values.push_back(value);
            if (value==test.level) {
                ++exactNodes;
                if (z+1<n && dot(test.normal,g.point(x,y,z+1))==test.level) ++exactGridEdges;
            }
        }
        verify(exactNodes>0,"fixture passes through exact grid vertices");
        if (test.normal.z==0) verify(exactGridEdges>0,"fixture contains exact grid edges");
        const auto triangles = equipotentials(g,test.level,false);
        verify(!triangles.empty() && triangles.size()%3==0,"triangle surface exists");
        std::map<std::array<Key,3>,int> faces;
        std::map<Edge,int> incidence;
        double totalArea = 0;
        for (size_t i=0;i<triangles.size();i+=3) {
            std::array<Key,3> face;
            for (int j=0;j<3;++j) {
                const Vec3 a = triangles[i+j].position;
                verify(std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z),"finite vertex coordinates");
                verify(std::abs(a.x)<=1 && std::abs(a.y)<=1 && std::abs(a.z)<=1,"vertex inside clipping box");
                verify(near(dot(test.normal,a),test.level,1e-12),"vertex on analytic affine plane");
                face[j] = key(a);
            }
            std::sort(face.begin(),face.end());
            verify(++faces[face]==1,"no duplicate faces");
            const Vec3 a=triangles[i].position, b=triangles[i+1].position, c=triangles[i+2].position;
            const double area = .5*length(cross(b-a,c-a));
            verify(std::isfinite(area) && area>0,"positive nondegenerate triangle area");
            totalArea += area;
            for (int j=0;j<3;++j) {
                Key first=key(triangles[i+j].position), second=key(triangles[i+(j+1)%3].position);
                if (second<first) std::swap(first,second);
                ++incidence[{first,second}];
            }
        }
        verify(near(totalArea,test.area,1e-10),"total area equals analytic clipped section");
        double boundaryLength = 0;
        int boundaryEdges = 0, interiorEdges = 0, sharedGridFaceEdges = 0;
        for (const auto& entry : incidence) {
            const Key a=entry.first.first, b=entry.first.second;
            bool boundary = false, sharedGridFace = false;
            for (int axis=0;axis<3;++axis) {
                boundary |= a[axis]==b[axis] && (a[axis]==-1 || a[axis]==1);
                // An edge on an interior grid face must match exactly from both sides.
                const double gridCoordinate=(a[axis]+1)*(n-1)/2;
                sharedGridFace |= a[axis]==b[axis] && a[axis]>-1 && a[axis]<1 &&
                    near(gridCoordinate,std::round(gridCoordinate),1e-12);
            }
            if (boundary) {
                verify(entry.second==1,"clipped boundary edge has one incident face");
                boundaryLength += length(Vec3{a[0]-b[0],a[1]-b[1],a[2]-b[2]});
                ++boundaryEdges;
            } else {
                verify(entry.second==2,"interior edge has exactly two incident faces");
                ++interiorEdges;
                if (sharedGridFace) ++sharedGridFaceEdges;
            }
        }
        verify(boundaryEdges>0 && interiorEdges>0 && sharedGridFaceEdges>0,"boundary and shared grid-face edges exercised");
        verify(near(boundaryLength,test.perimeter,1e-10),"boundary covers analytic section perimeter once");
        // Independent projected point coverage catches compensating holes/overlaps.
        // Irrational offsets keep probes away from the rational tessellation edges.
        auto project = [&](Vec3 a) { return std::array<double,2>{a.x,test.normal.z==0 ? a.z : a.y}; };
        auto orient = [](std::array<double,2> a,std::array<double,2> b,std::array<double,2> c) {
            return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
        };
        int coveredProbes = 0, outsideProbes = 0;
        for (int j=0;j<13;++j) for (int i=0;i<13;++i) {
            const double u=-1+2*(i+std::sqrt(2.0)/5)/13;
            const double v=-1+2*(j+std::sqrt(3.0)/5)/13;
            const Vec3 point = test.normal.z==0 ? Vec3{u,test.level-u,v} : Vec3{u,v,test.level-u-v};
            const bool inside = std::abs(point.y)<1 && std::abs(point.z)<1;
            const auto probe = project(point);
            int coverage = 0;
            for (size_t t=0;t<triangles.size();t+=3) {
                const auto a=project(triangles[t].position), b=project(triangles[t+1].position), c=project(triangles[t+2].position);
                const double denominator=orient(a,b,c);
                verify(denominator!=0,"nondegenerate projected triangle");
                const double wa=orient(b,c,probe)/denominator, wb=orient(c,a,probe)/denominator;
                const double wc=1-wa-wb;
                if (wa>=-1e-12 && wb>=-1e-12 && wc>=-1e-12) {
                    verify(wa>1e-12 && wb>1e-12 && wc>1e-12,"coverage probe avoids triangulation edges");
                    ++coverage;
                }
            }
            verify(coverage==(inside ? 1 : 0),"analytic section covered exactly once, outside uncovered");
            if (inside) ++coveredProbes; else ++outsideProbes;
        }
        verify(coveredProbes>50,"substantial interior coverage exercised");
        if (test.level!=0 || test.normal.z!=0) verify(outsideProbes>0,"outside clipped section exercised");
    }
}
void volumeTests() {
    Parameters p; p.volumeResolution=21;
    auto g=makeGrid3D(p);
    check(g.values.size()==21*21*21, "bounded volume count");
    check(near(g.point(0,0,0).x,-3) && near(g.point(20,20,20).z,3), "volume endpoints");
    for (int z=0;z<g.n;++z) for (int y=0;y<g.n;++y) for (int x=0;x<g.n;++x) {
        check(near(g.at(x,y,z),-g.at(g.n-1-x,y,z),1e-9), "volume reflection symmetry");
    }
    auto zero=equipotentials(g,0,true);
    check(!zero.empty(), "zero plane extracted");
    check(zero.size()==equipotentials(g,0,false).size(), "zero pair not duplicated");
    for (const auto& s:zero) check(near(s.position.x,0,1e-10), "bisector plane location");
    double zeroArea=0;
    for (size_t i=0;i<zero.size();i+=3) {
        double area=length(cross(zero[i+1].position-zero[i].position,zero[i+2].position-zero[i].position))*.5;
        check(std::isfinite(area) && area>0, "zero plane nondegenerate faces");
        zeroArea+=area;
    }
    check(near(zeroArea,36,1e-8), "clipped zero plane covers box cross section once");
    auto positive=equipotentials(g,4.5,false), negative=equipotentials(g,-4.5,false);
    check(!positive.empty() && !negative.empty(), "signed lobes extracted");
    check(equipotentials(g,4.5,true).size()==positive.size()+negative.size(), "paired signs");
    for (const auto& s:positive) check(s.position.x>0, "positive lobe side");
    for (const auto& s:negative) check(s.position.x<0, "negative lobe side");
    for (size_t i=0;i<positive.size();i+=3) {
        auto a=positive[i].position,b=positive[i+1].position,c=positive[i+2].position;
        check(std::isfinite(length(a)) && length(cross(b-a,c-a))>0, "finite nondegenerate triangles");
        check(std::abs(a.x)<=3 && std::abs(a.y)<=3 && std::abs(a.z)<=3, "vertices stay in box");
    }
    double maximum=0;
    for (double v:g.values) maximum=std::max(maximum,std::abs(v));
    check(equipotentials(g,maximum+1,true).empty(), "no crossing beyond sampled range");
    auto clipped=equipotentials(g,.1,false);
    bool boundary=false;
    for (const auto& s:clipped) boundary |= near(std::abs(s.position.y),3) || near(std::abs(s.position.z),3);
    check(boundary, "low level clipped at domain boundary");
    Parameters rotated=p; rotated.azimuth=37; rotated.polar=53; rotated.center={2,-1,3};
    auto rotatedPlane=equipotentials(makeGrid3D(rotated),0,true);
    auto axis=charges(rotated).plus-rotated.center;
    check(!rotatedPlane.empty(), "rotated translated plane extracted");
    for (const auto& s:rotatedPlane)
        check(std::abs(dot(s.position-rotated.center,axis))<.035, "rotated plane interpolation tolerance");
    double coarseResidual=0; int coarseCount=0;
    for (const auto& s:positive) if (length(s.position-charges(p).plus)>.7) {
        coarseResidual+=std::abs(potential(s.position,p)-4.5); ++coarseCount;
    }
    // A synthetic affine field exercises shared edges, exact nodes and coplanar faces.
    for (int z=0;z<g.n;++z) for (int y=0;y<g.n;++y) for (int x=0;x<g.n;++x)
        g.values[(z*g.n+y)*g.n+x]=g.point(x,y,z).x;
    auto plane=equipotentials(g,.123,false);
    for (const auto& s:plane) check(near(s.position.x,.123,1e-12), "linear edge interpolation");
    auto exact=equipotentials(g,0,false);
    std::map<std::array<double,9>,int> faces;
    double exactArea=0;
    for (size_t i=0;i<exact.size();i+=3) {
        exactArea+=length(cross(exact[i+1].position-exact[i].position,exact[i+2].position-exact[i].position))*.5;
        std::array<std::array<double,3>,3> pts;
        for (int j=0;j<3;++j) { auto a=exact[i+j].position; pts[j]={a.x,a.y,a.z}; }
        std::sort(pts.begin(),pts.end());
        std::array<double,9> key{};
        for (int j=0;j<3;++j) for (int k=0;k<3;++k) key[j*3+k]=pts[j][k];
        check(++faces[key]==1, "coplanar face emitted once");
    }
    check(near(exactArea,36,1e-8), "exact coplanar plane covers box once");
    for (auto& v:g.values) v=0;
    check(equipotentials(g,0,false).empty(), "constant volume has no arbitrary surface");
    p.volumeResolution=60; check(validate(p).empty(), "maximum volume resolution valid");
    auto fine=makeGrid3D(p);
    check(fine.values.size()==60*60*60, "maximum bounded allocation");
    auto triangles=equipotentials(fine,4.5,false);
    double fineResidual=0; int fineCount=0;
    for (const auto& s:triangles) if (length(s.position-charges(p).plus)>.7) {
        fineResidual+=std::abs(potential(s.position,p)-4.5); ++fineCount;
    }
    check(coarseCount>0 && fineCount>0 && fineResidual/fineCount<coarseResidual/coarseCount,
          "level residual improves with finer sampling");
    int tested=0;
    for (size_t i=0;i<triangles.size();i+=3) {
        auto a=triangles[i].position,b=triangles[i+1].position,c=triangles[i+2].position;
        if (length(a-charges(p).plus)<.7) continue;
        check(std::abs(potential(a,p)-4.5)<.12, "fine interpolated level residual");
        Vec3 e=field((a+b+c)*(1.0/3),p), n=cross(b-a,c-a);
        check(std::abs(dot(e,n))/(length(e)*length(n))>.97, "field normal to smooth interpolated surface");
        ++tested;
    }
    check(tested>100, "orthogonality exercised away from charge");
    check(field({2,0,0},p).x>0 && field({0,2,0},p).x<0, "3D field direction");
    p.volumeResolution=61; check(!validate(p).empty(), "reject oversized volume");
    p=Parameters{}; p.volumeHalfExtent=0; check(!validate(p).empty(), "reject empty volume");
    p=Parameters{}; p.potentialLevel=std::numeric_limits<double>::infinity();
    check(!validate(p).empty(), "reject nonfinite level");
    bool rejected=false;
    try { equipotentials(g,std::numeric_limits<double>::quiet_NaN(),false); }
    catch (const std::invalid_argument&) { rejected=true; }
    check(rejected, "reject invalid extraction level");
    g.values[0]=std::numeric_limits<double>::infinity(); rejected=false;
    try { equipotentials(g,0,false); } catch (const std::invalid_argument&) { rejected=true; }
    check(rejected, "reject nonfinite volume samples");
    p=Parameters{}; p.volumeHalfExtent=101; check(!validate(p).empty(), "bound volume extent");
    p=Parameters{}; p.potentialLevel=1e6+1; check(!validate(p).empty(), "bound numeric level");
    p=Parameters{}; p.volumeResolution=9; check(!validate(p).empty(), "bound minimum volume resolution");
}
void geometryTests() {
    Parameters p; p.resolution = 15;
    auto g = makeGrid(p);
    check(g.values.size() == 225, "grid count");
    check(near(g.point(0,0).x,-5) && near(g.point(14,14).y,5), "grid endpoints");
    auto segments = contours(g, 0);
    check(!segments.empty(), "zero equipotential exists");
    for (const auto& s : segments) {
        check(near(s.a.x,0) && near(s.b.x,0), "zero contour bisector");
        check(length(s.a-s.b)>1e-12, "no degenerate contour segments");
    }
    Grid flat{2,0,1,0,1,{0,0,0,0}};
    check(contours(flat,0).empty(), "constant cells have no arbitrary contour");
    Grid saddle{2,0,1,0,1,{1,-1,-1,1}};
    check(contours(saddle,0).size()==2, "saddle deterministic connectivity");
    Grid edge{2,0,1,0,1,{0,1,0,1}};
    check(contours(edge,0).size()==1, "exact edge contour retained");
    Grid tangent{2,0,1,0,1,{0,-1,-1,-1}};
    check(contours(tangent,0).empty(), "isolated exact vertex is not a line");
    bool invalidContour = false;
    try { contours(flat,std::numeric_limits<double>::infinity()); }
    catch (const std::invalid_argument&) { invalidContour = true; }
    check(invalidContour, "reject nonfinite contour level");
    p.sphereResolution = 100;
    auto s = sphere(p);
    check(s.size()==static_cast<size_t>(6*200*99), "closed sphere triangle count");
    bool north = false, south = false;
    for (const auto& a : s) {
        check(near(length(a.position-p.center),3,1e-9), "sphere constant radius");
        check(std::isfinite(a.value), "sphere finite potential");
        north |= near(a.position.z,3); south |= near(a.position.z,-3);
    }
    check(north && south, "sphere poles present");
    // Shared sample coordinates are exact because the azimuth wraps modulo slices.
    using Key = std::array<double,3>;
    using Edge = std::pair<Key,Key>;
    std::map<Edge,int> incidence;
    auto key = [](Vec3 a) { return Key{a.x,a.y,a.z}; };
    for (size_t i=0;i<s.size();i+=3) for (int e=0;e<3;++e) {
        Key a=key(s[i+e].position), b=key(s[i+(e+1)%3].position);
        if (b<a) std::swap(a,b);
        ++incidence[{a,b}];
    }
    for (const auto& edge : incidence) check(edge.second==2, "watertight seam and poles: two faces per edge");
    p.q = 5;
    auto scaledSphere = sphere(p);
    check(near(length(scaledSphere[123].position-s[123].position),0), "potential does not deform sphere");
    check(near(scaledSphere[123].value,5*s[123].value), "sphere color data scales with charge");
    // Every nondegenerate triangle has outward winding; seam is periodic.
    for (size_t i=0;i<s.size();i+=3) {
        auto a=s[i].position-p.center, b=s[i+1].position-p.center, c=s[i+2].position-p.center;
        auto u=b-a, v=c-a;
        Vec3 cross{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
        check(cross.x*a.x+cross.y*a.y+cross.z*a.z>0, "sphere winding and nondegenerate caps");
    }
    p.resolution = 150;
    g = makeGrid(p);
    auto h = heightfield(g,displayLimit(g.values),3);
    check(h.size()==static_cast<size_t>(6*149*149), "large heightfield without ushort indices");
    for (const auto& a : h) check(std::isfinite(a.position.z) && std::abs(a.position.z)<=3, "height clipping");
}
}
int main() {
    try { radialTests(); physicsTests(); singleChargeTests(); geometryTests(); volumeTests();
          affinePlaneTopologyTests(); streamlineTests(); monopoleStreamlineTests(); }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
    std::cout << "PASS: " << checks << " active behavior checks\n";
}
