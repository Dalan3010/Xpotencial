#include "controls.hpp"
#include "ui_text.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace app {
namespace {
constexpr float sidebar = 330;
struct Definition { const char* name; double low, high, step; bool integer; };
const Definition definitions[] = {
    {"Carga q (nC)",.1,5,.1,false},
    {"Coulomb k (x10^9 SI)",.5,20,.1,false},
    {"Separación d (m)",.2,6,.1,false},
    {"Acimut (grados)",0,360,1,false},
    {"Polar (grados)",0,180,1,false},
    {"Centro x (m)",-10,10,.5,false},
    {"Centro y (m)",-10,10,.5,false},
    {"Centro z / corte z (m)",-10,10,.5,false},
    {"Malla x mínimo (m)",-10,10,.5,false},
    {"Malla x máximo (m)",-10,10,.5,false},
    {"Malla y mínimo (m)",-10,10,.5,false},
    {"Malla y máximo (m)",-10,10,.5,false},
    {"Muestras por lado",15,150,1,true},
    {"Flechas de campo/lado",4,30,1,true},
};
double value(const dipole::Parameters& p, int index) {
    switch (index) {
    case 0: return p.q; case 1: return p.k; case 2: return p.d;
    case 3: return p.azimuth; case 4: return p.polar;
    case 5: return p.center.x; case 6: return p.center.y; case 7: return p.center.z;
    case 8: return p.xmin; case 9: return p.xmax; case 10: return p.ymin; case 11: return p.ymax;
    case 12: return p.resolution; default: return p.fieldDensity;
    }
}
void assign(dipole::Parameters& p, int index, double v) {
    switch (index) {
    case 0: p.q=v; break; case 1: p.k=v; break; case 2: p.d=v; break;
    case 3: p.azimuth=v; break; case 4: p.polar=v; break;
    case 5: p.center.x=v; break; case 6: p.center.y=v; break; case 7: p.center.z=v; break;
    case 8: p.xmin=v; break; case 9: p.xmax=v; break; case 10: p.ymin=v; break; case 11: p.ymax=v; break;
    case 12: p.resolution=static_cast<int>(v); break;
    case 13: p.fieldDensity=static_cast<int>(v); break;
    }
}
bool hit(Rectangle r) { return CheckCollisionPointRec(GetMousePosition(),r); }
bool button(Rectangle r,const char* text,bool selected=false) {
    DrawRectangleRec(r,selected ? Color{53,107,155,255} : hit(r) ? Color{65,78,96,255} : Color{40,50,67,255});
    app::DrawTextUi(text,static_cast<int>(r.x+8),static_cast<int>(r.y+8),16,RAYWHITE);
    return hit(r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}
}
Rectangle Controls::viewport() const {
    return {sidebar+15,100,static_cast<float>(GetScreenWidth())-sidebar-30,static_cast<float>(GetScreenHeight())-180};
}
bool Controls::apply(int index,double v) {
    if (!std::isfinite(v) || (definitions[index].integer && std::floor(v)!=v)) {
        error_ = "Use un número finito; las resoluciones deben ser enteras."; return false;
    }
    // Prevent out-of-range floating-to-integer conversion before validation.
    if (definitions[index].integer && (v<definitions[index].low || v>definitions[index].high)) {
        error_ = "Resolución / densidad fuera del rango mostrado."; return false;
    }
    auto candidate = parameters;
    assign(candidate,index,v);
    auto error = dipole::validate(candidate);
    if (!error.empty()) { error_ = error; return false; }
    error_.clear();
    if (value(parameters,index)==v) return false;
    parameters = candidate;
    return true;
}
bool Controls::parameterRow(int index,float y) {
    const auto& definition = definitions[index];
    app::DrawTextUi(definition.name,15,static_cast<int>(y),16,RAYWHITE);
    Rectangle number{155,y+23,112,26}, minus{272,y+23,22,26}, plus{298,y+23,22,26};
    char formatted[40]; std::snprintf(formatted,sizeof(formatted),"%.6g",value(parameters,index));
    bool changed = false;
    const bool inPanel = GetMouseX()<sidebar && GetMouseY()>=100 && GetMouseY()<GetScreenHeight()-70;
    DrawRectangleRec(number,editing_==index ? Color{64,93,119,255} : Color{31,40,54,255});
    app::DrawTextUi(editing_==index ? buffer_.c_str() : formatted,static_cast<int>(number.x+5),static_cast<int>(number.y+5),16,RAYWHITE);
    if (inPanel && hit(number) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        editing_ = index; buffer_ = formatted; selectAll_ = true; error_.clear();
    }
    if (button(minus,"-") && inPanel) { editing_=-1; changed |= apply(index,value(parameters,index)-definition.step); }
    if (button(plus,"+") && inPanel) { editing_=-1; changed |= apply(index,value(parameters,index)+definition.step); }
    Rectangle track{18,y+34,126,7};
    DrawRectangleRec(track,Color{74,86,101,255});
    float normalized = static_cast<float>(std::clamp((value(parameters,index)-definition.low)/(definition.high-definition.low),0.0,1.0));
    DrawCircle(static_cast<int>(track.x+normalized*track.width),static_cast<int>(track.y+3),6,SKYBLUE);
    Rectangle target{track.x-5,track.y-9,track.width+10,25};
    if (inPanel && hit(target) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { dragging_=index; editing_=-1; }
    if (dragging_==index && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        const double t = std::clamp((GetMouseX()-track.x)/track.width,0.0f,1.0f);
        double v = definition.low+t*(definition.high-definition.low);
        v = definition.low+std::round((v-definition.low)/definition.step)*definition.step;
        changed |= apply(index,std::clamp(v,definition.low,definition.high));
    }
    return changed;
}
bool Controls::draw(Rectangle plotRect) {
    bool changed = false;
    DrawRectangle(0,0,GetScreenWidth(),90,Color{24,32,46,255});
    const char* title = "Dipolo eléctrico | nativo C++17";
    app::DrawTextUi(title,15,12,22,RAYWHITE);
    // Source selector: two mutually exclusive buttons in the top bar, next to the title.
    const float sourceX = 15.0f + static_cast<float>(app::MeasureTextUi(title,22)) + 18.0f;
    if (button({sourceX,8,96,32},"Dipolo",parameters.chargeCount==2) && parameters.chargeCount!=2) {
        parameters.chargeCount = 2; changed = true;
    }
    if (button({sourceX+104,8,134,32},"Carga única",parameters.chargeCount==1) && parameters.chargeCount!=1) {
        parameters.chargeCount = 1; changed = true;
    }
    const char* tabs[] = {"Mapa de potencial","Equipotenciales","Campo vectorial","Superficie altura","Líneas campo 3D"};
    for (int i=0;i<5;++i) if (button({15.0f+i*148,47,142,32},tabs[i],view==i)) view=i;
    if (button({static_cast<float>(GetScreenWidth()-140),8,125,32},"Ayuda / física")) help=!help;
    DrawRectangle(0,90,static_cast<int>(sidebar),GetScreenHeight()-90,Color{24,32,46,255});
    const float areaHeight = static_cast<float>(GetScreenHeight()-170);
    const int rows=view==4 ? 8 : 14, toggles=view==4 ? 4 : 5;
    // Filter at render time; definition indices and their switches are never renumbered.
    const bool hideSeparation = parameters.chargeCount==1;
    auto hidden = [&](int index) { return hideSeparation && (index==2 || index==3 || index==4); };
    if (hidden(editing_)) editing_ = -1;
    if (hidden(dragging_)) dragging_ = -1;
    const int visibleRows = rows-(hideSeparation ? 3 : 0);
    const float contentHeight=static_cast<float>(visibleRows*68+toggles*32+92);
    if (!help && GetMouseX()<sidebar && GetMouseY()>100) scroll_ -= GetMouseWheelMove()*40;
    scroll_ = std::clamp(scroll_,0.0f,std::max(0.0f,contentHeight-areaHeight));
    BeginScissorMode(0,100,static_cast<int>(sidebar),static_cast<int>(areaHeight));
    float y = 106-scroll_;
    for (int index=0,rendered=0;index<rows;++index) {
        if (hidden(index)) continue;
        if (!help) changed |= parameterRow(index,y+rendered*68);
        else app::DrawTextUi(definitions[index].name,15,static_cast<int>(y+rendered*68),16,GRAY);
        ++rendered;
    }
    y += visibleRows*68;
    bool* planeFlags[] = {&visible.charges,&visible.labels,&visible.contours,&visible.grid,&visible.field};
    bool* streamFlags[] = {&visible.charges,&visible.labels,&visible.animationMarkers,&visible.animationPaused};
    const char* planeLabels[] = {"Mostrar cargas","Mostrar etiquetas de carga","Mostrar contornos","Mostrar malla","Campo superpuesto en mapa"};
    const char* streamLabels[] = {"Mostrar cargas","Mostrar etiquetas de carga","Mostrar marcadores","Pausar animación de marcadores"};
    bool** flags=view==4 ? streamFlags : planeFlags;
    const char** labels=view==4 ? streamLabels : planeLabels;
    for (int i=0;i<toggles;++i) {
        Rectangle r{15,y+i*32,300,27};
        app::DrawTextUi(*flags[i] ? "[x]" : "[ ]",15,static_cast<int>(r.y+4),17,SKYBLUE);
        app::DrawTextUi(labels[i],52,static_cast<int>(r.y+4),17,RAYWHITE);
        if (!help && hit(r) && GetMouseY()>=100 && GetMouseY()<100+areaHeight && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            *flags[i]=!*flags[i];
        }
    }
    y += toggles*32+8;
    if (view==4) {
        app::DrawTextUi("Velocidad",15,static_cast<int>(y+2),15,RAYWHITE);
        Rectangle speedTrack{118,y+8,175,7};
        DrawRectangleRec(speedTrack,Color{74,86,101,255});
        DrawCircle(static_cast<int>(speedTrack.x+(visible.animationSpeed-.25f)/1.75f*speedTrack.width),static_cast<int>(speedTrack.y+3),6,SKYBLUE);
        Rectangle speedHit{speedTrack.x-5,speedTrack.y-9,speedTrack.width+10,25};
        if (hit(speedHit) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && GetMouseX()<sidebar) dragging_=100;
        if (dragging_==100 && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            visible.animationSpeed=std::clamp(.25f+(GetMouseX()-speedTrack.x)/speedTrack.width*1.75f,.25f,2.0f);
        char speed[32]; std::snprintf(speed,sizeof(speed),"%.2gx",visible.animationSpeed);
        app::DrawTextUi(speed,297,static_cast<int>(y+1),14,LIGHTGRAY);
        y+=31;
    }
    if (button({15,y,300,33},"Restaurar valores") && !help && GetMouseY()>=100 && GetMouseY()<100+areaHeight) {
        // Keep exploring the current source: the reset restores parameters, not the selector.
        const int source = parameters.chargeCount;
        parameters = dipole::Parameters{}; parameters.chargeCount = source;
        visible = Visibility{}; editing_=-1; dragging_=-1; error_.clear(); changed=true;
    }
    EndScissorMode();
    if (contentHeight>areaHeight) {
        float thumb = areaHeight*areaHeight/contentHeight;
        DrawRectangle(324,100+static_cast<int>(scroll_/(contentHeight-areaHeight)*(areaHeight-thumb)),4,static_cast<int>(thumb),SKYBLUE);
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) dragging_=-1;
    if (editing_>=0 && !help) {
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_A)) selectAll_=true;
        for (int c=GetCharPressed();c>0;c=GetCharPressed()) {
            if ((c>='0' && c<='9') || c=='.' || c=='-' || c=='+' || c=='e' || c=='E') {
                if (selectAll_) { buffer_.clear(); selectAll_=false; }
                if (buffer_.size()<24) buffer_+=static_cast<char>(c);
            }
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            if (selectAll_) { buffer_.clear(); selectAll_=false; }
            else if (!buffer_.empty()) buffer_.pop_back();
        }
        if (IsKeyPressed(KEY_ESCAPE)) editing_=-1;
        if (IsKeyPressed(KEY_ENTER)) {
            char* end = nullptr;
            const double v = std::strtod(buffer_.c_str(),&end);
            if (end==buffer_.c_str() || *end!='\0') error_="Número inválido. Intro para aplicar; Esc para cancelar.";
            else {
                changed |= apply(editing_,v);
                if (error_.empty()) editing_=-1;
            }
        }
    }
    app::DrawTextUi("Desplazar panel | clic en valor para escribir",12,GetScreenHeight()-58,15,LIGHTGRAY);
    app::DrawTextUi("Intro: aplicar  /  Esc: cancelar",12,GetScreenHeight()-37,15,LIGHTGRAY);
    if (!error_.empty()) app::DrawTextUi(error_.c_str(),345,GetScreenHeight()-28,16,ORANGE);
    if (help) drawHelp(plotRect);
    return changed;
}
void Controls::probePoint(const Rectangle& plotRect) {
    // Sample the cut plane only where the plot is actually drawn; otherwise keep
    // the previous physical point so the numbers stay stable between frames.
    if (plotRect.width>1 && plotRect.height>1 && CheckCollisionPointRec(GetMousePosition(),plotRect)) {
        const double spanX=parameters.xmax-parameters.xmin, spanY=parameters.ymax-parameters.ymin;
        const double tx=std::clamp(static_cast<double>(GetMouseX()-plotRect.x)/plotRect.width,0.0,1.0);
        const double ty=std::clamp(static_cast<double>(plotRect.y+plotRect.height-GetMouseY())/plotRect.height,0.0,1.0);
        probe_={parameters.xmin+tx*spanX,parameters.ymin+ty*spanY,parameters.center.z};
        hasProbe_=true;
    }
    if (!hasProbe_)
        probe_={(parameters.xmin+parameters.xmax)/2,(parameters.ymin+parameters.ymax)/2,parameters.center.z};
}
void Controls::drawHelp(const Rectangle& plotRect) {
    probePoint(plotRect);
    const bool single=parameters.chargeCount==1;
    const double epsilon=dipole::softening(parameters);
    const dipole::Charges c=dipole::charges(parameters);
    const double rawPlus=dipole::length(probe_-c.plus), rawMinus=dipole::length(probe_-c.minus);
    // Distances include the same softening the core applies before dividing.
    const double rPlus=std::sqrt(rawPlus*rawPlus+epsilon*epsilon);
    const double rMinus=std::sqrt(rawMinus*rawMinus+epsilon*epsilon);
    const double potential=dipole::potential(probe_,parameters);
    const dipole::Vec3 field=dipole::field(probe_,parameters);
    const double fieldMagnitude=dipole::length(field);
    const double moment=parameters.q*parameters.d;
    const Rectangle vp=viewport();
    const float panelWidth=std::min(560.0f,std::max(360.0f,vp.width-140.0f));
    const float panelHeight=std::min(480.0f,vp.height-10.0f);
    Rectangle panel{vp.x+vp.width-panelWidth,vp.y+8,panelWidth,panelHeight};
    DrawRectangleRec(panel,Color{20,29,43,244});
    DrawRectangleLinesEx(panel,1,Color{53,107,155,255});
    const int x=static_cast<int>(panel.x+16);
    float y=panel.y+14;
    auto drawLine=[&](const char* text,int size,Color color,float advance) {
        app::DrawTextUi(text,x,static_cast<int>(y),size,color); y+=advance;
    };
    char b[256];
    drawLine("Formulas y resultados en tiempo real",19,SKYBLUE,26);
    drawLine(view<3 ? "Mueve el raton sobre el mapa del corte para sondear el punto."
                    : "Vista 3D: no hay corte, se evalua el ultimo punto sondeado.",
             13,LIGHTGRAY,22);
    y+=4;
    std::snprintf(b,sizeof b,"Punto de evaluacion (corte z = %.4g m)",probe_.z);
    drawLine(b,14,RAYWHITE,21);
    std::snprintf(b,sizeof b,"x = %.4g m   y = %.4g m   z = %.4g m",probe_.x,probe_.y,probe_.z);
    drawLine(b,14,RAYWHITE,21);
    if (single) std::snprintf(b,sizeof b,"r = %.6g m",rPlus);
    else std::snprintf(b,sizeof b,"r+ = %.6g m   r- = %.6g m",rPlus,rMinus);
    drawLine(b,14,RAYWHITE,21);
    y+=6;
    std::snprintf(b,sizeof b,"p = q * d = (%.4g nC)(%.4g m) = %.4g nC*m",parameters.q,parameters.d,moment);
    drawLine(b,14,Color{255,214,120,255},22);
    if (single) std::snprintf(b,sizeof b,"V = k * q * (1/r) = %.5g V",potential);
    else std::snprintf(b,sizeof b,"V = k * q * (1/r+ - 1/r-) = %.5g V",potential);
    drawLine(b,14,Color{150,222,150,255},22);
    std::snprintf(b,sizeof b,"|E| = |-grad V| = %.5g V/m",fieldMagnitude);
    drawLine(b,14,Color{150,222,150,255},22);
    std::snprintf(b,sizeof b,"E = (%.4g, %.4g, %.4g) V/m",field.x,field.y,field.z);
    drawLine(b,14,LIGHTGRAY,22);
    if (single) std::snprintf(b,sizeof b,"epsilon = max(0.001 * L, 0.000001) = %.4g m",epsilon);
    else std::snprintf(b,sizeof b,"epsilon = max(0.001 * d, 0.000001) = %.4g m",epsilon);
    drawLine(b,14,Color{255,214,120,255},22);
    y+=6;
    app::DrawTextUi("Convenciones: q en nC; k mostrada en unidades SI de 10^9.",x,static_cast<int>(y),13,LIGHTGRAY); y+=19;
    app::DrawTextUi("E = -grad V con diferencias centradas, h = 0.05 * epsilon.",x,static_cast<int>(y),13,LIGHTGRAY); y+=19;
    app::DrawTextUi("Corte de mapa y campo: z fisico = z del centro.",x,static_cast<int>(y),13,LIGHTGRAY); y+=19;
    app::DrawTextUi(single ? "Vista 5 integra el campo de carga unica." : "Vista 5 integra el campo del dipolo.",
                    x,static_cast<int>(y),13,LIGHTGRAY);
}
}
