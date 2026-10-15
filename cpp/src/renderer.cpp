#include "ui_text.hpp"
#include "renderer.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <new>
#include <stdexcept>

namespace app {
namespace {
Color color(double value,double limit) {
    const float t = static_cast<float>(std::clamp(value/limit,-1.0,1.0));
    const Color neutral{242,243,236,255}, positive{211,48,56,255}, negative{33,87,181,255};
    const Color end = t>=0 ? positive : negative;
    const float a = std::abs(t);
    return {static_cast<unsigned char>(neutral.r+(end.r-neutral.r)*a),
            static_cast<unsigned char>(neutral.g+(end.g-neutral.g)*a),
            static_cast<unsigned char>(neutral.b+(end.b-neutral.b)*a),255};
}
Vector3 world(dipole::Vec3 a,dipole::Vec3 origin) {
    a=a-origin;
    return {static_cast<float>(a.x),static_cast<float>(a.z),static_cast<float>(-a.y)};
}
Mesh gpuMesh(const dipole::Triangles& triangles,double limit,dipole::Vec3 origin) {
    Mesh mesh{};
    mesh.vertexCount=static_cast<int>(triangles.size());
    mesh.triangleCount=mesh.vertexCount/3;
    mesh.vertices=static_cast<float*>(MemAlloc(static_cast<unsigned int>(triangles.size()*3*sizeof(float))));
    mesh.colors=static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(triangles.size()*4)));
    if (!mesh.vertices || !mesh.colors) {
        MemFree(mesh.vertices); MemFree(mesh.colors); throw std::bad_alloc();
    }
    for (size_t i=0;i<triangles.size();++i) {
        const auto a=world(triangles[i].position,origin);
        mesh.vertices[i*3]=a.x; mesh.vertices[i*3+1]=a.y; mesh.vertices[i*3+2]=a.z;
        const Color c=color(triangles[i].value,limit);
        mesh.colors[i*4]=c.r; mesh.colors[i*4+1]=c.g; mesh.colors[i*4+2]=c.b; mesh.colors[i*4+3]=255;
    }
    // No indices: vertexCount can safely exceed 65535.
    UploadMesh(&mesh,false);
    if (!mesh.vaoId || !mesh.vboId) {
        UnloadMesh(mesh);
        throw std::runtime_error("Falló la carga de la malla a la GPU (se requiere OpenGL 3.3).");
    }
    return mesh;
}
void legend(Rectangle viewport,double limit) {
    const int x=static_cast<int>(viewport.x+viewport.width-25), y=static_cast<int>(viewport.y+35);
    const int height=std::max(80,static_cast<int>(viewport.height)-120);
    for (int i=0;i<height;++i) DrawRectangle(x,y+i,16,1,color(limit*(1-2.0*i/(height-1)),limit));
    char text[70];
    std::snprintf(text,sizeof(text),"+%.3g V",limit); app::DrawTextUi(text,x-87,y,15,DARKGRAY);
    app::DrawTextUi("0 V",x-42,y+height/2,15,DARKGRAY);
    app::DrawTextUi(text,x-87,y+height-15,15,DARKGRAY);
    app::DrawTextUi("Color recortado",x-100,y+height+8,13,DARKGRAY);
}
void lineArrow(Vector2 a,Vector2 b,Color c) {
    Vector2 d=Vector2Subtract(b,a);
    if (Vector2Length(d)<1) return;
    d=Vector2Normalize(d);
    DrawLineEx(a,b,1.6f,c);
    Vector2 back=Vector2Subtract(b,Vector2Scale(d,6));
    Vector2 side{-d.y*3,d.x*3};
    DrawTriangle(b,Vector2Add(back,side),Vector2Subtract(back,side),c);
    // A line head is visible regardless of triangle culling winding.
    DrawLineEx(b,Vector2Add(back,side),1.5f,c);
    DrawLineEx(b,Vector2Subtract(back,side),1.5f,c);
}
// The lone charge takes its colour and label from the sign of q; the dipole keeps +q/-q.
bool chargeIsPositive(const dipole::Parameters& p,int index) {
    return p.chargeCount==1 ? p.q>=0 : index==0;
}
}
Renderer::Renderer() { material_=LoadMaterialDefault(); }
Renderer::~Renderer() {
    if (heatmap_.id) UnloadTexture(heatmap_);
    if (surface_.vertices) UnloadMesh(surface_);
    if (target_.id) UnloadRenderTexture(target_);
    // The default shader and default white texture are raylib-owned, not ours.
    // Only the map array allocated by LoadMaterialDefault belongs to this instance.
    MemFree(material_.maps);
}
void Renderer::update(const dipole::Parameters& p) {
    const auto error=dipole::validate(p);
    if (!error.empty()) throw std::invalid_argument(error);
    const bool physicsChanged=!initialized_ || p.q!=p_.q || p.k!=p_.k || p.d!=p_.d ||
        p.chargeCount!=p_.chargeCount || p.azimuth!=p_.azimuth || p.polar!=p_.polar ||
        p.center.x!=p_.center.x || p.center.y!=p_.center.y || p.center.z!=p_.center.z;
    const bool planeChanged=physicsChanged || p.xmin!=p_.xmin || p.xmax!=p_.xmax ||
        p.ymin!=p_.ymin || p.ymax!=p_.ymax || p.resolution!=p_.resolution || p.fieldDensity!=p_.fieldDensity;
    if (physicsChanged) {
        dipole::StreamlineOptions options;
        options.seedCount=16;
        options.seedAngleDegrees=72;
        options.stepSize=.02;
        streamlines_=dipole::streamlines(p,options);
        animationPhase_=0;
    }
    p_=p;
    initialized_=true;
    if (!planeChanged) return;
    grid_=dipole::makeGrid(p);
    gridLimit_=dipole::displayLimit(grid_.values);
    contours_.clear();
    for (int i=-8;i<=8;++i) {
        const double level=gridLimit_*i/8;
        auto segments=dipole::contours(grid_,level);
        contours_.insert(contours_.end(),segments.begin(),segments.end());
    }
    arrows_.clear(); fieldMax_=0;
    const int n=p.fieldDensity;
    for (int y=0;y<n;++y) for (int x=0;x<n;++x) {
        dipole::Vec3 point{p.xmin+(p.xmax-p.xmin)*(x+.5)/n,p.ymin+(p.ymax-p.ymin)*(y+.5)/n,p.center.z};
        auto e=dipole::field(point,p);
        fieldMax_=std::max(fieldMax_,dipole::length(e));
        arrows_.push_back({point,e});
    }
    const double height=.3*std::min(p.xmax-p.xmin,p.ymax-p.ymin);
    const auto surfaceData=dipole::heightfield(grid_,gridLimit_,height);
    if (surface_.vertices) { UnloadMesh(surface_); surface_={}; }
    surface_=gpuMesh(surfaceData,gridLimit_,{(p.xmin+p.xmax)/2,(p.ymin+p.ymax)/2,0});
    std::vector<Color> pixels(static_cast<size_t>(grid_.n*grid_.n));
    for (int y=0;y<grid_.n;++y) for (int x=0;x<grid_.n;++x)
        pixels[static_cast<size_t>(y*grid_.n+x)]=color(grid_.at(x,grid_.n-1-y),gridLimit_);
    Image image{pixels.data(),grid_.n,grid_.n,1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    if (heatmap_.id) { UnloadTexture(heatmap_); heatmap_={}; }
    heatmap_=LoadTextureFromImage(image); // Image borrows vector memory; do not UnloadImage.
    if (!heatmap_.id) throw std::runtime_error("Falló la asignación de la textura del mapa de calor.");
    SetTextureFilter(heatmap_,TEXTURE_FILTER_BILINEAR);
}
void Renderer::drawPlane(Rectangle r,int view,const Visibility& v) {
    // Fit physical domain with equal x/y scale; do not stretch distances.
    const double spanX=p_.xmax-p_.xmin, spanY=p_.ymax-p_.ymin;
    const float scale=static_cast<float>(std::min((r.width-160)/spanX,(r.height-80)/spanY));
    Rectangle plot{r.x+55+(r.width-160-static_cast<float>(spanX)*scale)/2,
                   r.y+30+(r.height-80-static_cast<float>(spanY)*scale)/2,
                   static_cast<float>(spanX)*scale,static_cast<float>(spanY)*scale};
    auto screen = [&](dipole::Vec3 p) {
        return Vector2{plot.x+static_cast<float>(p.x-p_.xmin)*scale,plot.y+plot.height-static_cast<float>(p.y-p_.ymin)*scale};
    };
    if (view!=1) DrawTexturePro(heatmap_,{0,0,static_cast<float>(heatmap_.width),static_cast<float>(heatmap_.height)},plot,{0,0},0,WHITE);
    else DrawRectangleRec(plot,Color{248,247,241,255});
    BeginScissorMode(static_cast<int>(plot.x),static_cast<int>(plot.y),static_cast<int>(plot.width)+1,static_cast<int>(plot.height)+1);
    if (v.grid) for (int i=0;i<=10;++i) {
        const float x=plot.x+plot.width*i/10, y=plot.y+plot.height*i/10;
        DrawLineEx({x,plot.y},{x,plot.y+plot.height},1,Color{70,80,100,45});
        DrawLineEx({plot.x,y},{plot.x+plot.width,y},1,Color{70,80,100,45});
    }
    if (v.contours && view!=2) for (const auto& segment : contours_)
        DrawLineEx(screen(segment.a),screen(segment.b),segment.level==0 ? 2.0f : 1.1f,
                   view==1 ? color(segment.level,gridLimit_) : Color{38,44,59,120});
    if (view==2 || (view==0 && v.field)) {
        const double scale=dipole::referenceLength(p_);
        const double reference=p_.k*p_.q/(scale*scale);
        const double maxLength=.045*std::min(spanX,spanY); // Independent of arrow density.
        for (const auto& a : arrows_) {
            const double magnitude=std::hypot(a.field.x,a.field.y);
            if (magnitude<1e-12) continue;
            auto offset=dipole::Vec3{a.field.x,a.field.y,0}*(maxLength/(magnitude+reference));
            lineArrow(screen(a.point-offset*.5),screen(a.point+offset*.5),Color{12,38,53,255});
        }
    }
    if (v.charges) {
        auto c=dipole::charges(p_);
        // One marker in single-charge mode, two in dipole mode (indices are never renumbered).
        for (int i=0;i<p_.chargeCount;++i) {
            auto point=screen(i==0 ? c.plus : c.minus);
            const bool positive=chargeIsPositive(p_,i);
            DrawCircleV(point,7,positive ? MAROON : DARKBLUE);
            DrawCircleLines(static_cast<int>(point.x),static_cast<int>(point.y),7,WHITE);
            if (v.labels) app::DrawTextUi(positive ? "+q" : "-q",static_cast<int>(point.x+9),static_cast<int>(point.y-18),18,BLACK);
        }
    }
    EndScissorMode();
    DrawRectangleLinesEx(plot,1,DARKGRAY);
    for (int i=0;i<=4;++i) {
        char text[32];
        std::snprintf(text,sizeof(text),"%.3g",p_.xmin+spanX*i/4);
        app::DrawTextUi(text,static_cast<int>(plot.x+plot.width*i/4)-10,static_cast<int>(plot.y+plot.height+8),14,DARKGRAY);
        std::snprintf(text,sizeof(text),"%.3g",p_.ymin+spanY*i/4);
        app::DrawTextUi(text,static_cast<int>(plot.x)-48,static_cast<int>(plot.y+plot.height-plot.height*i/4)-7,14,DARKGRAY);
    }
    app::DrawTextUi("x (m)",static_cast<int>(plot.x+plot.width/2)-20,static_cast<int>(plot.y+plot.height+28),16,DARKGRAY);
    app::DrawTextUi("y (m)",static_cast<int>(plot.x)-45,static_cast<int>(plot.y)-22,16,DARKGRAY);
    char caption[170];
    std::snprintf(caption,sizeof(caption),"Corte físico z = %.3g m | color +/- %.3g V (recorte 95%%)",p_.center.z,gridLimit_);
    app::DrawTextUi(caption,static_cast<int>(r.x+8),static_cast<int>(r.y+5),15,DARKGRAY);
    legend(r,gridLimit_);
    if (view==2 || (view==0 && v.field)) {
        std::snprintf(caption,sizeof(caption),"Flechas XY | máx |E| muestreado %.3g V/m | longitud satura al 4.5%% de la extensión",fieldMax_);
        app::DrawTextUi(caption,static_cast<int>(r.x+8),static_cast<int>(r.y+r.height+9),14,DARKGRAY);
    } else if (view==1) app::DrawTextUi("17 niveles de -límite de color a +límite de color; nivel cero en negrita",static_cast<int>(r.x+8),static_cast<int>(r.y+r.height+9),14,DARKGRAY);
}
void Renderer::drawSpatial(Rectangle r,int view,const Visibility& v,bool blocked) {
    const bool isStreamlines=view==4;
    const int cameraIndex=isStreamlines ? 1 : 0;
    const bool onPlot=CheckCollisionPointRec(GetMousePosition(),r);
    if (blocked || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) orbiting_[cameraIndex]=false;
    if (!blocked && onPlot && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) orbiting_[cameraIndex]=true;
    if (orbiting_[cameraIndex] && onPlot && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        auto delta=GetMouseDelta();
        yaw_[cameraIndex]-=delta.x*.007f;
        pitch_[cameraIndex]=std::clamp(pitch_[cameraIndex]+delta.y*.007f,-1.4f,1.4f);
    }
    if (!blocked && onPlot) zoom_[cameraIndex]=std::clamp(zoom_[cameraIndex]*std::exp(-GetMouseWheelMove()*.12f),.5f,8.0f);
    const int w=std::max(1,static_cast<int>(r.width)), h=std::max(1,static_cast<int>(r.height));
    if (target_.texture.width!=w || target_.texture.height!=h) {
        if (target_.id) UnloadRenderTexture(target_);
        target_=LoadRenderTexture(w,h);
        if (!target_.id) throw std::runtime_error("Falló la asignación del destino de renderizado 3D.");
    }
    const double span=std::max(p_.xmax-p_.xmin,p_.ymax-p_.ymin);
    const dipole::Vec3 origin=isStreamlines ? p_.center : dipole::Vec3{(p_.xmin+p_.xmax)/2,(p_.ymin+p_.ymax)/2,0};
    float extent=static_cast<float>(span);
    Vector3 streamCenter{};
    float streamRadius=0;
    if (isStreamlines) {
        Vector3 minimum{INFINITY,INFINITY,INFINITY}, maximum{-INFINITY,-INFINITY,-INFINITY};
        bool hasPoints=false;
        for (const auto& path : streamlines_) for (const auto point : path.points) {
            const Vector3 position=world(point,origin);
            minimum={std::min(minimum.x,position.x),std::min(minimum.y,position.y),std::min(minimum.z,position.z)};
            maximum={std::max(maximum.x,position.x),std::max(maximum.y,position.y),std::max(maximum.z,position.z)};
            hasPoints=true;
        }
        if (hasPoints) {
            streamCenter=Vector3Scale(Vector3Add(minimum,maximum),.5f);
            for (const auto& path : streamlines_) for (const auto point : path.points)
                streamRadius=std::max(streamRadius,Vector3Distance(world(point,origin),streamCenter));
        }
        extent=std::max(streamRadius*2,static_cast<float>(dipole::referenceLength(p_)));
    }
    const float viewRadius=extent*1.65f;
    const Vector3 direction{std::cos(pitch_[cameraIndex])*std::cos(yaw_[cameraIndex]),
                            std::sin(pitch_[cameraIndex]),
                            std::cos(pitch_[cameraIndex])*std::sin(yaw_[cameraIndex])};
    float radius=viewRadius*zoom_[cameraIndex];
    if (isStreamlines && streamRadius>0) {
        constexpr float verticalFov=45.0f*DEG2RAD;
        const float aspect=static_cast<float>(w)/h;
        const float limitingHalfFov=std::atan(std::tan(verticalFov*.5f)*std::min(1.0f,aspect));
        radius=streamRadius/std::sin(limitingHalfFov)*1.12f*zoom_[cameraIndex];
    }
    Camera3D camera{};
    camera.position=Vector3Add(streamCenter,Vector3Scale(direction,radius));
    camera.target=streamCenter; camera.up={0,1,0}; camera.fovy=45; camera.projection=CAMERA_PERSPECTIVE;
    BeginTextureMode(target_);
    ClearBackground(Color{243,245,248,255});
    const double previousNear=rlGetCullDistanceNear(), previousFar=rlGetCullDistanceFar();
    // Fit the supported meter domain, including very small and very large spans.
    rlSetClipPlanes(std::max(1e-7,static_cast<double>(extent)*.001),radius+extent*4);
    if (isStreamlines && !v.animationPaused) animationPhase_+=GetFrameTime()*.42*v.animationSpeed;
    BeginMode3D(camera);
    if (isStreamlines) {
        for (const auto& path : streamlines_) {
            for (size_t i=1;i<path.points.size();++i) {
                DrawLine3D(world(path.points[i-1],origin),world(path.points[i],origin),Color{92,98,104,235});
            }
        }
    } else DrawMesh(surface_,material_,MatrixIdentity());
    if (!isStreamlines && v.grid) for (int i=0;i<=10;++i) {
        const float half=extent/2, a=-half+extent*i/10;
        DrawLine3D({-half,0,a},{half,0,a},Color{80,95,110,70});
        DrawLine3D({a,0,-half},{a,0,half},Color{80,95,110,70});
    }
    if (!isStreamlines && v.contours) {
        const double height=.3*std::min(p_.xmax-p_.xmin,p_.ymax-p_.ymin);
        for (auto segment : contours_) {
            segment.a.z=segment.b.z=std::clamp(segment.level/gridLimit_,-1.0,1.0)*height+.002*span;
            DrawLine3D(world(segment.a,origin),world(segment.b,origin),Color{28,35,48,180});
        }
    }
    const float axis=extent*.65f;
    DrawLine3D({0,0,0},{axis,0,0},RED);
    DrawLine3D({0,0,0},{0,0,-axis},GREEN);
    DrawLine3D({0,0,0},{0,axis,0},BLUE);
    std::array<Vector3,2> chargePoints{};
    const bool showCharges=v.charges;
    if (showCharges) {
        auto c=dipole::charges(p_);
        for (int i=0;i<p_.chargeCount;++i) {
            auto point=i==0 ? c.plus : c.minus;
            if (!isStreamlines) {
                point.z=.3*std::min(p_.xmax-p_.xmin,p_.ymax-p_.ymin)*std::clamp(dipole::potential({point.x,point.y,p_.center.z},p_)/gridLimit_,-1.0,1.0);
            }
            chargePoints[i]=world(point,origin);
            DrawSphere(chargePoints[i],extent*.012f,chargeIsPositive(p_,i) ? MAROON : DARKBLUE);
        }
    }
    if (isStreamlines && v.animationMarkers) for (const auto& path : streamlines_) {
        if (path.points.size()<2) continue;
        std::vector<double> lengths(path.points.size(),0);
        for (size_t i=1;i<path.points.size();++i)
            lengths[i]=lengths[i-1]+dipole::length(path.points[i]-path.points[i-1]);
        const double total=lengths.back();
        if (total<=0) continue;
        const double markerScale=dipole::referenceLength(p_);
        for (int marker=0;marker<2;++marker) {
            const double distance=std::fmod(animationPhase_*markerScale+marker*.5*total,total);
            const auto upper=std::lower_bound(lengths.begin(),lengths.end(),distance);
            const size_t index=std::clamp<size_t>(static_cast<size_t>(upper-lengths.begin()),1,lengths.size()-1);
            const double segment=lengths[index]-lengths[index-1];
            const double t=segment>0 ? (distance-lengths[index-1])/segment : 0;
            const auto point=path.points[index-1]+(path.points[index]-path.points[index-1])*t;
            DrawSphere(world(point,origin),static_cast<float>(dipole::referenceLength(p_)*.018),Color{155,160,164,230});
        }
    }
    EndMode3D();
    auto label=[&](Vector3 a,const char* text,Color c) {
        const Vector2 pos=GetWorldToScreenEx(a,camera,w,h);
        if (pos.x>=0 && pos.x<w && pos.y>=0 && pos.y<h) app::DrawTextUi(text,static_cast<int>(pos.x+5),static_cast<int>(pos.y),16,c);
    };
    char xLabel[64],yLabel[64],zLabel[64];
    std::snprintf(xLabel,sizeof(xLabel),"x %.3g m",origin.x+axis);
    std::snprintf(yLabel,sizeof(yLabel),"y %.3g m",origin.y+axis);
    std::snprintf(zLabel,sizeof(zLabel),isStreamlines ? "z %.3g m" : "z %.3g visual",origin.z+axis);
    label({axis,0,0},xLabel,MAROON); label({0,0,-axis},yLabel,DARKGREEN);
    label({0,axis,0},isStreamlines ? zLabel : "altura visual V",DARKBLUE);
    if (showCharges && v.labels) {
        label(chargePoints[0],chargeIsPositive(p_,0) ? "+q" : "-q",chargeIsPositive(p_,0) ? MAROON : DARKBLUE);
        if (p_.chargeCount!=1) label(chargePoints[1],"-q",DARKBLUE);
    }
    EndTextureMode();
    rlSetClipPlanes(previousNear,previousFar);
    DrawTexturePro(target_.texture,{0,0,static_cast<float>(w),-static_cast<float>(h)},r,{0,0},0,WHITE);
    char caption[180];
    if (isStreamlines) {
        std::snprintf(caption,sizeof(caption),"Líneas de flujo del campo eléctrico integradas | %zu rutas | metros físicos",streamlines_.size());
    } else std::snprintf(caption,sizeof(caption),"ALTURA visual = %.3g * clamp(V/%.3g, -1, 1); NO es z físico",.3*std::min(p_.xmax-p_.xmin,p_.ymax-p_.ymin),gridLimit_);
    app::DrawTextUi(caption,static_cast<int>(r.x+8),static_cast<int>(r.y+8),15,DARKGRAY);
    if (isStreamlines) {
        app::DrawTextUi(p_.chargeCount==1
            ? "Los marcadores indican la dirección del campo hacia fuera, no la velocidad real."
            : "Los marcadores indican la dirección del campo de +q a -q, no la velocidad real.",
            static_cast<int>(r.x+8),static_cast<int>(r.y+30),14,DARKGRAY);
        app::DrawTextUi("Arrastra para orbitar; rueda para zoom. Las curvas físicas en caché no cambian con la animación.",
            static_cast<int>(r.x+8),static_cast<int>(r.y+r.height+9),14,DARKGRAY);
    } else {
        app::DrawTextUi("x/y en metros; marcadores XY sobre la superficie. Arrastra para orbitar; rueda para zoom.",
            static_cast<int>(r.x+8),static_cast<int>(r.y+r.height+9),14,DARKGRAY);
        legend(r,gridLimit_);
    }
}
void Renderer::draw(Rectangle viewport,int view,const Visibility& visibility,bool cameraBlocked) {
    DrawRectangleRec(viewport,Color{249,250,252,255});
    if (view<3) drawPlane(viewport,view,visibility);
    else drawSpatial(viewport,view,visibility,cameraBlocked);
}
}
