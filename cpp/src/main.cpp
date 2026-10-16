#include "ui_text.hpp"
#include "controls.hpp"
#include "renderer.hpp"
#include <cstring>
#include <algorithm>
#include <exception>
#include <iostream>

int main(int argc,char** argv) {
    bool smoke=false;
    for (int i=1;i<argc;++i) {
        if (std::strcmp(argv[i],"--smoke")==0) smoke=true;
        else if (std::strcmp(argv[i],"--help")==0) {
            std::cout << "dipole_simulator [--smoke]\n--smoke abre una ventana y renderiza las cinco vistas, luego sale.\n";
            return 0;
        } else { std::cerr << "Argumento desconocido: " << argv[i] << '\n'; return 2; }
    }
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1200,700,"Dipolo eléctrico - simulador nativo");
    if (!IsWindowReady()) { std::cerr << "No se puede crear una ventana gráfica.\n"; return 1; }
    app::InitUiFont();
    SetWindowMinSize(1000,640);
    SetExitKey(KEY_NULL); // Escape cancels numerical editing, not the application.
    SetTargetFPS(60);
    int result=0;
    try {
        app::Controls controls;
        if (smoke) {
            controls.parameters.resolution=150;
            controls.parameters.fieldDensity=30;
        }
        app::Renderer renderer; // GPU resources are destroyed before CloseWindow.
        renderer.update(controls.parameters);
        int titleMode = controls.parameters.chargeCount;
        bool dirty=false;
        int frame=0;
        constexpr int framesPerView=8;
        constexpr int smokeStages=5;
        const double smokeStart=GetTime();
        const char* views[]={"mapa de potencial","contornos equipotenciales","campo vectorial","superficie de altura","líneas de flujo del dipolo"};
        while (!WindowShouldClose()) {
            if (smoke) {
                if (GetTime()-smokeStart>25) { std::cerr << "La prueba smoke superó el tiempo máximo.\n"; result=1; break; }
                const int stage=frame/framesPerView;
                controls.view=std::min(stage,4);
                if (frame%framesPerView==0) {
                    std::cout << "SMOKE: renderizando " << views[stage] << '\n';
                }
            }
            if (dirty) { renderer.update(controls.parameters); dirty=false; }
            BeginDrawing();
            ClearBackground(Color{232,236,242,255});
            renderer.draw(controls.viewport(),controls.view,controls.visible,controls.blocksCamera());
            dirty=controls.draw(renderer.plotRectangle());
            if (controls.parameters.chargeCount!=titleMode) {
                titleMode=controls.parameters.chargeCount;
                SetWindowTitle(titleMode==1 ? "Carga única - simulador nativo" : "Dipolo eléctrico - simulador nativo");
            }
            EndDrawing();
            ++frame;
            if (smoke && frame==smokeStages*framesPerView) break;
        }
        if (smoke) {
            if (frame==smokeStages*framesPerView) std::cout << "SMOKE PASS: las cinco vistas renderizadas, incluidas las líneas de flujo del dipolo en caché y los marcadores de dirección animados, 40 fotogramas.\n";
            else { std::cerr << "Prueba smoke interrumpida antes de renderizar todas las vistas.\n"; result=1; }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error de simulación: " << e.what() << '\n'; result=1;
    }
    app::UnloadUiFont();
    CloseWindow();
    return result;
}
