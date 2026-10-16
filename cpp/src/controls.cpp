#include "controls.hpp"
#include "ui_text.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

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
std::string fixed(double v,int decimals) {
    char b[64];
    std::snprintf(b,sizeof b,"%.*f",decimals,v);
    return std::string(b);
}
// Latin-1-safe scientific notation: the UI only loads codepoints 32..255, so
// superscript digits such as 10^9 are written in plain ASCII.
std::string scientific(double v,int decimals) {
    int exponent=0;
    double mantissa=v;
    if (std::fabs(v)>0.0) {
        exponent=static_cast<int>(std::floor(std::log10(std::fabs(v))));
        mantissa=v/std::pow(10.0,exponent);
        if (std::fabs(mantissa)>=10.0) { mantissa/=10.0; ++exponent; }
        else if (std::fabs(mantissa)<1.0) { mantissa*=10.0; --exponent; }
    }
    char b[64];
    std::snprintf(b,sizeof b,"%.*fx10^%d",decimals,mantissa,exponent);
    return std::string(b);
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
    // Single-charge view 5 has no markers or animation: only charges + labels stay.
    const bool streamSingle = view==4 && parameters.chargeCount==1;
    const int rows=view==4 ? 8 : 14, toggles=view==4 ? (streamSingle ? 2 : 4) : 5;
    // Filter at render time; definition indices and their switches are never renumbered.
    const bool hideSeparation = parameters.chargeCount==1;
    auto hidden = [&](int index) { return hideSeparation && (index==2 || index==3 || index==4); };
    if (hidden(editing_)) editing_ = -1;
    if (hidden(dragging_)) dragging_ = -1;
    const int visibleRows = rows-(hideSeparation ? 3 : 0);
    // 92 = reset button + speed row + footer hints; the speed row adds 31 when shown.
    const float contentHeight=static_cast<float>(visibleRows*68+toggles*32+(streamSingle ? 61 : 92));
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
    if (view==4 && !streamSingle) {
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
    const float panelWidth=std::min(760.0f,std::max(360.0f,vp.width-140.0f));
    const float panelHeight=std::min(560.0f,vp.height-10.0f);
    Rectangle panel{vp.x+vp.width-panelWidth,vp.y+8,panelWidth,panelHeight};
    DrawRectangleRec(panel,Color{20,29,43,244});
    DrawRectangleLinesEx(panel,1,Color{53,107,155,255});
    const int x=static_cast<int>(panel.x+16);
    float y=panel.y+14;

    const Color formula{120,220,255,255};
    const Color result{57,255,20,255};
    const Color label{200,206,216,255};
    const Color muted{150,156,168,255};

    struct Seg { std::string text; Color color; bool highlight=false; };
    auto plain=[&](const std::string& text,int size,Color color,float advance) {
        app::DrawTextUi(text.c_str(),x,static_cast<int>(y),size,color); y+=advance;
    };
    // Renders one equation line, shrinking the font until it fits the panel and
    // emphasising the highlighted (final-result) segment in neon green, faux bold.
    auto equation=[&](const std::vector<Seg>& segs,float advance) {
        auto widthAt=[&](int size) {
            float w=0;
            for (const auto& s:segs) w+=static_cast<float>(app::MeasureTextUi(s.text.c_str(),s.highlight ? size+1 : size));
            return w;
        };
        int size=14;
        const float available=panel.width-32.0f;
        while (size>10 && widthAt(size)>available) --size;
        float cx=static_cast<float>(x);
        for (const auto& s:segs) {
            const int px=static_cast<int>(cx), sz=s.highlight ? size+1 : size;
            app::DrawTextUi(s.text.c_str(),px,static_cast<int>(y),sz,s.color);
            if (s.highlight) app::DrawTextUi(s.text.c_str(),px+1,static_cast<int>(y),sz,s.color);
            cx+=static_cast<float>(app::MeasureTextUi(s.text.c_str(),sz));
        }
        y+=advance;
    };

    plain("Formulas y resultados en tiempo real",19,SKYBLUE,26);
    plain(view<3 ? "Mueve el raton sobre el mapa del corte para sondear el punto."
                 : "Vista 3D: no hay corte, se evalua el ultimo punto sondeado.",13,muted,22);
    y+=4;
    plain("Punto de evaluacion (corte z = "+fixed(probe_.z,4)+" m)",14,label,21);
    plain("x = "+fixed(probe_.x,4)+" m   y = "+fixed(probe_.y,4)+" m   z = "+fixed(probe_.z,4)+" m",14,label,21);
    if (single) plain("r = "+fixed(rPlus,5)+" m",14,label,21);
    else plain("r+ = "+fixed(rPlus,5)+" m   r- = "+fixed(rMinus,5)+" m",14,label,21);
    y+=6;
    equation({{"p = q·d",formula},
              {"  ==>  p = ",muted},
              {"("+fixed(parameters.q,2)+" nC) · ("+fixed(parameters.d,2)+" m)",formula},
              {" = ",muted},
              {fixed(moment,2)+" nC·m",result,true}},22);
    if (single) {
        equation({{"V = k·q·(1/r)",formula},
                  {"  ==>  V = ",muted},
                  {"("+scientific(parameters.k,2)+") · ("+fixed(parameters.q,2)+" nC) · (1/"+fixed(rPlus,2)+")",formula},
                  {" = ",muted},
                  {fixed(potential,2)+" V",result,true}},22);
    } else {
        equation({{"V = k·q·(1/r+ - 1/r-)",formula},
                  {"  ==>  V = ",muted},
                  {"("+scientific(parameters.k,2)+") · ("+fixed(parameters.q,2)+" nC) · (1/"+fixed(rPlus,2)+" - 1/"+fixed(rMinus,2)+")",formula},
                  {" = ",muted},
                  {fixed(potential,2)+" V",result,true}},22);
    }
    equation({{"|E| = |-grad V|",formula},
              {"  ==>  |E| = |(Ex, Ey, Ez)| = sqrt(Ex²+Ey²+Ez²)",formula},
              {" = ",muted},
              {fixed(fieldMagnitude,2)+" V/m",result,true}},22);
    plain("E = ("+fixed(field.x,3)+", "+fixed(field.y,3)+", "+fixed(field.z,3)+") V/m",13,muted,20);
    if (single) plain("epsilon = max(0.001 · L, 1e-6) = "+fixed(epsilon,5)+" m",13,muted,20);
    else plain("epsilon = max(0.001 · d, 1e-6) = "+fixed(epsilon,5)+" m",13,muted,20);
    y+=6;
    plain("Convenciones: q en nC; k mostrada en unidades SI de 10^9.",13,muted,19);
    plain("E = -grad V con diferencias centradas, h = 0.05 * epsilon.",13,muted,19);
    plain("Corte de mapa y campo: z fisico = z del centro.",13,muted,19);
    plain(single ? "Vista 5 dibuja 5 esferas equipotenciales concéntricas alrededor de la carga."
                 : "Vista 5 integra el campo del dipolo.",13,muted,19);
}
}
