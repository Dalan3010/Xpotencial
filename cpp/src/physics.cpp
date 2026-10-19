#include "dipole/physics.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace dipole {
namespace {
constexpr double pi = 3.14159265358979323846;
void require(const Parameters& p) {
    auto error = validate(p);
    if (!error.empty()) throw std::invalid_argument(error);
}
bool finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
double evaluate(Vec3 point, const Parameters& p, const Charges& c) {
    const double eps = softening(p);
    auto distance = [&](Vec3 charge) {
        Vec3 r = point-charge;
        return std::sqrt(r.x*r.x+r.y*r.y+r.z*r.z+eps*eps);
    };
    // Single charge: skip the negative term so every derived view stays correct.
    if (p.chargeCount == 1) return p.k*p.q*(1/distance(c.plus));
    return p.k*p.q*(1/distance(c.plus)-1/distance(c.minus));
}
}
Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 operator*(Vec3 a, double s) { return {a.x*s,a.y*s,a.z*s}; }
double length(Vec3 a) { return std::hypot(a.x,a.y,a.z); }
std::string validate(const Parameters& p) {
    if (p.chargeCount != 1 && p.chargeCount != 2) return "El número de cargas debe ser 1 o 2.";
    const double numbers[] = {p.q,p.k,p.d,p.azimuth,p.polar,p.xmin,p.xmax,p.ymin,p.ymax,p.sphereRadius,p.potentialLevel,p.volumeHalfExtent,p.samplingRadius,p.visualRadius,p.fieldContrast};
    for (double v : numbers) if (!std::isfinite(v)) return "Todos los valores deben ser finitos.";
    if (!finite(p.center)) return "El centro debe ser finito.";
    if (p.q<.1 || p.q>5 || p.k<.5 || p.k>20 || p.d<.2 || p.d>6) return "q, k o d fuera del rango admitido.";
    if (p.azimuth<0 || p.azimuth>360 || p.polar<0 || p.polar>180) return "Ángulos fuera del rango admitido.";
    // Bound world coordinates to retain useful precision when converting to GPU floats.
    if (std::abs(p.center.x)>10000 || std::abs(p.center.y)>10000 || std::abs(p.center.z)>10000)
        return "El centro debe estar dentro de +/-10000 m.";
    if (p.xmin>=p.xmax || p.ymin>=p.ymax) return "Cada mínimo debe ser menor que su máximo.";
    if (std::abs(p.xmin)>10000 || std::abs(p.xmax)>10000 || std::abs(p.ymin)>10000 || std::abs(p.ymax)>10000)
        return "El dominio debe estar dentro de +/-10000 m.";
    if (p.xmax-p.xmin<.001 || p.ymax-p.ymin<.001) return "La extensión del dominio debe ser al menos 0.001 m.";
    if (p.resolution<15 || p.resolution>150 || p.fieldDensity<4 || p.fieldDensity>30)
        return "Resolución de malla 15..150; densidad de campo 4..30.";
    if (p.sphereRadius<.5 || p.sphereRadius>10 || p.sphereResolution<10 || p.sphereResolution>100)
        return "Radio de esfera 0.5..10 m; resolución 10..100.";
    if (std::abs(p.potentialLevel)>1e6 || p.volumeHalfExtent<.1 || p.volumeHalfExtent>100 ||
        p.volumeResolution<10 || p.volumeResolution>60)
        return "Nivel 3D +/-1e6 V; semiextensión 0.1..100 m; resolución 10..60.";
    if (p.samplingRadius<.5 || p.samplingRadius>20 || p.visualRadius<.1 || p.visualRadius>10 ||
        p.fieldContrast<0 || p.fieldContrast>.9 || p.angularResolution<10 || p.angularResolution>80)
        return "Distancia de sonda 0.5..20 m; radio visual 0.1..10; contraste 0..0.9; resolución angular 10..80.";
    return {};
}
Charges charges(const Parameters& p) {
    // Single charge: plus is the sole charge, minus is unused.
    if (p.chargeCount == 1) return {p.center,p.center};
    const double az = p.azimuth*pi/180, pol = p.polar*pi/180;
    Vec3 offset{std::sin(pol)*std::cos(az)*p.d/2,std::sin(pol)*std::sin(az)*p.d/2,std::cos(pol)*p.d/2};
    return {p.center+offset,p.center-offset};
}
double referenceLength(const Parameters& p) {
    if (p.chargeCount == 1) return .5*std::min(p.xmax-p.xmin,p.ymax-p.ymin);
    return p.d;
}
double softening(const Parameters& p) { return std::max(referenceLength(p)*.001,1e-6); }
double potential(Vec3 point, const Parameters& p) {
    require(p);
    if (!finite(point)) throw std::invalid_argument("El punto de evaluación debe ser finito.");
    return evaluate(point,p,charges(p));
}
Vec3 field(Vec3 point, const Parameters& p) {
    require(p);
    if (!finite(point)) throw std::invalid_argument("El punto de evaluación debe ser finito.");
    const double h = std::max(1e-6,softening(p)*.05);
    const auto c = charges(p);
    auto derivative = [&](Vec3 delta) { return -(evaluate(point+delta,p,c)-evaluate(point-delta,p,c))/(2*h); };
    return {derivative({h,0,0}),derivative({0,h,0}),derivative({0,0,h})};
}
std::vector<Streamline> streamlines(const Parameters& p, const StreamlineOptions& options) {
    require(p);
    const double values[] = {options.seedAngleDegrees, options.seedRadius, options.stepSize,
        options.endpointRadius, options.escapeRadius, options.maxArcLength};
    for (double value : values) if (!std::isfinite(value))
        throw std::invalid_argument("Las opciones de línea de flujo deben ser finitas.");
    if (options.seedCount < 1 || options.seedCount > 128 || options.maxSteps < 1 || options.maxSteps > 100000 ||
        options.seedAngleDegrees <= 0 || options.seedAngleDegrees >= 179 ||
        options.seedRadius < .005 || options.seedRadius > .45 || options.stepSize <= 0 || options.stepSize > .1 ||
        options.endpointRadius <= 0 || options.endpointRadius > .5 || options.escapeRadius <= 0 || options.escapeRadius > 1000 ||
        options.maxArcLength <= 0 || options.maxArcLength > 1000)
        throw std::invalid_argument("Opciones de línea de flujo fuera de los límites admitidos.");

    const auto pair = charges(p);
    const bool monopole = p.chargeCount == 1;
    const double reference = referenceLength(p);
    auto unit = [](Vec3 vector) {
        const double magnitude = length(vector);
        if (!std::isfinite(magnitude) || magnitude <= 1e-30)
            throw std::runtime_error("No se puede integrar un campo eléctrico nulo o no finito.");
        return vector * (1.0 / magnitude);
    };
    // Only a dipole has a pair axis to seed a ring about; the monopole uses a sphere.
    Vec3 axis{}, transverse{}, second{};
    if (!monopole) {
        axis = (pair.plus - pair.minus) * (1.0 / p.d);
        const Vec3 referenceAxis = std::abs(axis.z) < .9 ? Vec3{0,0,1} : Vec3{0,1,0};
        transverse = unit({referenceAxis.y*axis.z-referenceAxis.z*axis.y,
            referenceAxis.z*axis.x-referenceAxis.x*axis.z, referenceAxis.x*axis.y-referenceAxis.y*axis.x});
        second = {axis.y*transverse.z-axis.z*transverse.y,
            axis.z*transverse.x-axis.x*transverse.z, axis.x*transverse.y-axis.y*transverse.x};
    }
    const double theta = options.seedAngleDegrees * pi / 180;
    const double seedRadius = options.seedRadius * reference;
    const double stepSize = options.stepSize * reference;
    const double endpointRadius = options.endpointRadius * reference;
    const double escapeRadius = options.escapeRadius * reference;
    const double maxArcLength = options.maxArcLength * reference;
    auto directionAt = [&](Vec3 position) { return unit(field(position, p)); };
    std::vector<Streamline> result;
    result.reserve(static_cast<size_t>(options.seedCount));
    // Equal-area directions on the unit sphere: deterministic and axis independent.
    constexpr double goldenAngle = 2.39996322972865332; // pi * (3 - sqrt(5)).
    for (int seed = 0; seed < options.seedCount; ++seed) {
        Vec3 radial{};
        if (monopole) {
            const double y = 1 - 2*(seed+.5)/options.seedCount;
            const double ring = std::sqrt(std::max(0.0,1-y*y));
            const double phi = goldenAngle*seed;
            radial = {std::cos(phi)*ring,std::sin(phi)*ring,y};
        } else {
            const double phi = 2*pi*seed/options.seedCount;
            radial = axis*std::cos(theta) +
                (transverse*std::cos(phi) + second*std::sin(phi))*std::sin(theta);
        }
        Streamline path;
        // charges() returns the center in both modes, so both seed from it.
        Vec3 position = pair.plus + radial*seedRadius;
        path.points.push_back(position);
        double arcLength = 0;
        for (int step = 0; step < options.maxSteps; ++step) {
            // A single charge has no partner to sink into; it only escapes or runs out of budget.
            if (!monopole && length(position-pair.minus) <= endpointRadius) {
                path.termination = StreamlineTermination::ReachedNegativeCharge;
                break;
            }
            if (length(position-p.center) >= escapeRadius) {
                path.termination = StreamlineTermination::Escaped;
                break;
            }
            if (arcLength >= maxArcLength) {
                path.termination = StreamlineTermination::ArcLengthLimit;
                break;
            }
            const double h = std::min(stepSize, maxArcLength-arcLength);
            const Vec3 k1 = directionAt(position);
            const Vec3 k2 = directionAt(position+k1*(h*.5));
            const Vec3 k3 = directionAt(position+k2*(h*.5));
            const Vec3 k4 = directionAt(position+k3*h);
            const Vec3 increment = (k1+k2*2+k3*2+k4)*(h/6);
            position = position + increment;
            if (!finite(position)) throw std::runtime_error("La integración de la línea de flujo produjo un punto no finito.");
            arcLength += length(increment);
            path.points.push_back(position);
            if (step+1 == options.maxSteps) path.termination = StreamlineTermination::StepLimit;
        }
        result.push_back(std::move(path));
    }
    return result;
}
Vec3 Grid::point(int x, int y) const {
    return {xmin+(xmax-xmin)*x/(n-1),ymin+(ymax-ymin)*y/(n-1),0};
}
double Grid::at(int x, int y) const { return values.at(static_cast<size_t>(y*n+x)); }
Grid makeGrid(const Parameters& p) {
    require(p);
    Grid g{p.resolution,p.xmin,p.xmax,p.ymin,p.ymax,{}};
    g.values.reserve(static_cast<size_t>(g.n*g.n));
    auto c = charges(p);
    for (int y=0;y<g.n;++y) for (int x=0;x<g.n;++x) {
        Vec3 point = g.point(x,y); point.z = p.center.z;
        g.values.push_back(evaluate(point,p,c));
    }
    return g;
}
std::vector<Segment> contours(const Grid& g, double level) {
    if (g.n<2 || g.values.size()!=static_cast<size_t>(g.n*g.n) || !std::isfinite(level))
        throw std::invalid_argument("Malla de contornos o nivel inválidos.");
    for (double value : g.values) if (!std::isfinite(value)) throw std::invalid_argument("Muestra de contorno no finita.");
    std::vector<Segment> result;
    std::set<std::tuple<double,double,double,double>> seen;
    auto emit = [&](Vec3 a, Vec3 b) {
        if (length(a-b)<=1e-12) return;
        if (std::tie(a.x,a.y)>std::tie(b.x,b.y)) std::swap(a,b);
        if (seen.emplace(a.x,a.y,b.x,b.y).second) result.push_back({a,b,level});
    };
    for (int y=0;y<g.n-1;++y) for (int x=0;x<g.n-1;++x) {
        const std::array<Vec3,4> pts{g.point(x,y),g.point(x+1,y),g.point(x+1,y+1),g.point(x,y+1)};
        const std::array<double,4> v{g.at(x,y)-level,g.at(x+1,y)-level,g.at(x+1,y+1)-level,g.at(x,y+1)-level};
        if (std::all_of(v.begin(),v.end(),[](double a){return a==0;})) continue;
        std::vector<Vec3> hits;
        for (int e=0;e<4;++e) {
            const int next = (e+1)%4;
            if (v[e]==0 && v[next]==0) { emit(pts[e],pts[next]); continue; }
            // Zero is assigned to the positive side consistently. Interpolation still uses exact zero.
            if ((v[e]>=0)==(v[next]>=0)) continue;
            const double t = v[e]/(v[e]-v[next]);
            hits.push_back(pts[e]+(pts[next]-pts[e])*t);
        }
        if (hits.size()==2) emit(hits[0],hits[1]);
        else if (hits.size()==4) {
            // Asymptotic decider. Exact saddles use a fixed, deterministic tie rule.
            const double determinant = v[0]*v[2]-v[1]*v[3];
            if (determinant>=0) { emit(hits[0],hits[1]); emit(hits[2],hits[3]); }
            else { emit(hits[0],hits[3]); emit(hits[1],hits[2]); }
        }
    }
    return result;
}
AngularField sampleFieldAngles(const Parameters& p) {
    require(p);
    AngularField out;
    out.resolution=p.angularResolution;
    out.effectiveRadius=std::max(p.samplingRadius,.75*p.d);
    out.minimum=std::numeric_limits<double>::infinity();
    const int n=out.resolution, slices=2*n;
    auto add=[&](Vec3 direction) {
        Vec3 e=field(p.center+direction*out.effectiveRadius,p);
        const double magnitude=length(e);
        if (!finite(e) || !std::isfinite(magnitude)) throw std::invalid_argument("Campo angular no finito.");
        out.samples.push_back({direction,e,magnitude});
        out.minimum=std::min(out.minimum,magnitude); out.maximum=std::max(out.maximum,magnitude);
    };
    add({0,0,1});
    for (int t=1;t<n;++t) for (int f=0;f<slices;++f) {
        const double theta=pi*t/n, phi=2*pi*f/slices;
        add({std::sin(theta)*std::cos(phi),std::sin(theta)*std::sin(phi),std::cos(theta)});
    }
    add({0,0,-1});
    return out;
}
RadialSurface radialSurface(const AngularField& angular,double base,double contrast) {
    const int n=angular.resolution, slices=2*n;
    if (n<10 || n>80 || angular.samples.size()!=static_cast<size_t>(2+(n-1)*slices) ||
        !std::isfinite(base) || base<.1 || base>10 || !std::isfinite(contrast) || contrast<0 || contrast>.9 ||
        !std::isfinite(angular.effectiveRadius) || angular.effectiveRadius<=0 ||
        !std::isfinite(angular.minimum) || !std::isfinite(angular.maximum) ||
        angular.minimum<0 || angular.maximum<angular.minimum)
        throw std::invalid_argument("Parámetros de superficie radial o retícula angular inválidos.");
    RadialSurface out;
    const double range=angular.maximum-angular.minimum;
    // Relative numerical contrast tolerance preserves uniform q/k scaling.
    const bool uniform=range<=1e-8*std::max(angular.maximum,1e-30);
    for (const auto& a:angular.samples) {
        if (!finite(a.direction) || !finite(a.field) || !std::isfinite(a.magnitude) ||
            a.magnitude<angular.minimum || a.magnitude>angular.maximum || std::abs(length(a.direction)-1)>1e-8)
            throw std::invalid_argument("Muestra angular inválida.");
        const double s=uniform ? .5 : (a.magnitude-angular.minimum)/range;
        out.vertices.push_back({a.direction*(base*(1+contrast*(2*s-1))),a.magnitude});
    }
    auto index=[&](int t,int f)->size_t {
        if (t==0) return 0;
        if (t==n) return out.vertices.size()-1;
        return static_cast<size_t>(1+(t-1)*slices+f%slices);
    };
    for (int t=0;t<n;++t) for (int f=0;f<slices;++f) {
        size_t a=index(t,f), b=index(t+1,f), c=index(t+1,f+1), d=index(t,f+1);
        if (t!=n-1) out.faces.push_back({a,b,c});
        if (t!=0) out.faces.push_back({a,c,d});
        out.wires.push_back({a,b});
        if (t>0) out.wires.push_back({a,d});
    }
    return out;
}
Vec3 Grid3D::point(int x, int y, int z) const {
    return {minimum.x+(maximum.x-minimum.x)*x/(n-1),
            minimum.y+(maximum.y-minimum.y)*y/(n-1),
            minimum.z+(maximum.z-minimum.z)*z/(n-1)};
}
double Grid3D::at(int x, int y, int z) const {
    return values.at(static_cast<size_t>((z*n+y)*n+x));
}
Grid3D makeGrid3D(const Parameters& p) {
    require(p);
    Vec3 half{p.volumeHalfExtent,p.volumeHalfExtent,p.volumeHalfExtent};
    Grid3D g{p.volumeResolution,p.center-half,p.center+half,{}};
    g.values.reserve(static_cast<size_t>(g.n*g.n*g.n));
    auto c=charges(p);
    for (int z=0;z<g.n;++z) for (int y=0;y<g.n;++y) for (int x=0;x<g.n;++x)
        g.values.push_back(evaluate(g.point(x,y,z),p,c));
    return g;
}
Triangles equipotentials(const Grid3D& g, double level, bool paired) {
    if (g.n<2 || g.n>60 || g.values.size()!=static_cast<size_t>(g.n*g.n*g.n) ||
        !finite(g.minimum) || !finite(g.maximum) || !std::isfinite(level) || std::abs(level)>1e6 ||
        g.minimum.x>=g.maximum.x || g.minimum.y>=g.maximum.y || g.minimum.z>=g.maximum.z)
        throw std::invalid_argument("Volumen o nivel de potencial inválidos.");
    for (double v:g.values) if (!std::isfinite(v)) throw std::invalid_argument("Muestra de volumen no finita.");
    Triangles out;
    auto cross=[](Vec3 a,Vec3 b) { return Vec3{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; };
    auto dot=[](Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; };
    // Consistent body diagonal 0--6; neighboring cube faces have the same split.
    constexpr int tetrahedra[6][4]={{0,1,2,6},{0,2,3,6},{0,3,7,6},{0,7,4,6},{0,4,5,6},{0,5,1,6}};
    constexpr int corners[8][3]={{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
    using Key=std::array<double,3>;
    const double spacing=std::min({g.maximum.x-g.minimum.x,g.maximum.y-g.minimum.y,g.maximum.z-g.minimum.z})/(g.n-1);
    for (int sign=0;sign<(paired && level!=0 ? 2 : 1);++sign) {
        const double target=sign==0 ? level : -level;
        std::set<std::array<Key,3>> seen;
        for (int z=0;z<g.n-1;++z) for (int y=0;y<g.n-1;++y) for (int x=0;x<g.n-1;++x) {
            std::array<Vec3,8> pts;
            std::array<double,8> values;
            std::array<int,8> ids;
            for (int i=0;i<8;++i) {
                int xx=x+corners[i][0], yy=y+corners[i][1], zz=z+corners[i][2];
                pts[i]=g.point(xx,yy,zz); values[i]=g.at(xx,yy,zz)-target;
                ids[i]=(zz*g.n+yy)*g.n+xx;
            }
            if (*std::min_element(values.begin(),values.end())>0 || *std::max_element(values.begin(),values.end())<0) continue;
            for (const auto& tet:tetrahedra) {
                std::vector<Vec3> hits;
                auto add=[&](Vec3 a) {
                    for (Vec3 b:hits) if (a.x==b.x && a.y==b.y && a.z==b.z) return;
                    hits.push_back(a);
                };
                bool allZero=true;
                for (int i:tet) { if (values[i]==0) add(pts[i]); else allZero=false; }
                // A constant tetrahedron has volume, not a uniquely defined level surface.
                if (allZero) continue;
                for (int i=0;i<4;++i) for (int j=i+1;j<4;++j) {
                    int a=tet[i], b=tet[j];
                    if ((values[a]<0 && values[b]>0) || (values[a]>0 && values[b]<0)) {
                        // Canonical endpoint order gives identical interpolation on shared edges.
                        if (ids[a]>ids[b]) std::swap(a,b);
                        double t=values[a]/(values[a]-values[b]);
                        add(pts[a]+(pts[b]-pts[a])*std::clamp(t,0.0,1.0));
                    }
                }
                if (hits.size()<3) continue;
                Vec3 normal=cross(hits[1]-hits[0],hits[2]-hits[0]);
                if (length(normal)<=spacing*spacing*1e-14) continue;
                Vec3 center{};
                for (auto a:hits) center=center+a;
                center=center*(1.0/hits.size());
                Vec3 u=hits[0]-center, v=cross(normal,u);
                u=u*(1/length(u)); v=v*(1/length(v));
                std::sort(hits.begin(),hits.end(),[&](Vec3 a,Vec3 b) {
                    return std::atan2(dot(a-center,v),dot(a-center,u))<std::atan2(dot(b-center,v),dot(b-center,u));
                });
                for (size_t i=1;i+1<hits.size();++i) {
                    Vec3 a=hits[0], b=hits[i], c=hits[i+1];
                    if (!finite(a) || !finite(b) || !finite(c) || length(cross(b-a,c-a))<=spacing*spacing*1e-14) continue;
                    std::array<Key,3> key{{{a.x,a.y,a.z},{b.x,b.y,b.z},{c.x,c.y,c.z}}};
                    std::sort(key.begin(),key.end());
                    if (seen.insert(key).second) out.insert(out.end(),{{a,target},{b,target},{c,target}});
                }
            }
        }
    }
    return out;
}
Triangles sphere(const Parameters& p) {
    require(p);
    const int rings = p.sphereResolution, slices = 2*rings;
    Triangles out;
    out.reserve(static_cast<size_t>(6*slices*(rings-1)));
    auto c = charges(p);
    auto sample = [&](int t, int f) {
        const double theta = pi*t/rings, phi = 2*pi*(f%slices)/slices;
        Vec3 offset{p.sphereRadius*std::sin(theta)*std::cos(phi),p.sphereRadius*std::sin(theta)*std::sin(phi),p.sphereRadius*std::cos(theta)};
        if (t==0) offset = {0,0,p.sphereRadius};
        if (t==rings) offset = {0,0,-p.sphereRadius};
        Vec3 point = p.center+offset;
        return Sample{point,evaluate(point,p,c)};
    };
    auto triangle = [&](Sample a,Sample b,Sample d) { out.push_back(a); out.push_back(b); out.push_back(d); };
    for (int t=0;t<rings;++t) for (int f=0;f<slices;++f) {
        auto a=sample(t,f), b=sample(t+1,f), c1=sample(t+1,f+1), d=sample(t,f+1);
        if (t!=rings-1) triangle(a,b,c1);
        if (t!=0) triangle(a,c1,d);
    }
    return out;
}
Triangles heightfield(const Grid& g, double limit, double height) {
    if (g.n<2 || g.values.size()!=static_cast<size_t>(g.n*g.n) || !std::isfinite(limit) || limit<=0 || !std::isfinite(height) || height<=0)
        throw std::invalid_argument("Campo de altura inválido.");
    Triangles out;
    out.reserve(static_cast<size_t>(6*(g.n-1)*(g.n-1)));
    auto sample = [&](int x,int y) {
        Vec3 point = g.point(x,y);
        const double value = g.at(x,y);
        if (!std::isfinite(value)) throw std::invalid_argument("Muestra de altura no finita.");
        const double t = std::clamp(std::abs(value/limit), 0.0, 1.0);
        const double exponent = 0.5;
        point.z = std::copysign(std::pow(t, exponent), value) * height;
        return Sample{point,value};
    };
    for (int y=0;y<g.n-1;++y) for (int x=0;x<g.n-1;++x) {
        auto a=sample(x,y),b=sample(x+1,y),c=sample(x+1,y+1),d=sample(x,y+1);
        out.insert(out.end(),{a,b,c,a,c,d});
    }
    return out;
}
double displayLimit(const std::vector<double>& values) {
    if (values.empty()) return 1;
    std::vector<double> sorted;
    sorted.reserve(values.size());
    for (double v : values) {
        if (!std::isfinite(v)) throw std::invalid_argument("Muestra de color no finita.");
        sorted.push_back(std::abs(v));
    }
    const size_t index = static_cast<size_t>(.95*(sorted.size()-1));
    std::nth_element(sorted.begin(),sorted.begin()+index,sorted.end());
    return std::max(sorted[index],1e-9);
}
}
