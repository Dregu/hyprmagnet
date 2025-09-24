#define WLR_USE_UNSTABLE

#include <unistd.h>
#include <re2/re2.h>
#include <string>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/desktop/LayerSurface.hpp>
#include "hyprland/src/helpers/memory/Memory.hpp"

inline HANDLE PHANDLE = nullptr;

// Methods
inline CFunctionHook* g_pMouseMotionHook = nullptr;
typedef void (*origMotion)(CSeatManager*, uint32_t, const Vector2D&);

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

void hkNotifyMotion(CSeatManager* thisptr, uint32_t time_msec, const Vector2D& local) {
    static auto       PMARGIN = CConfigValue<Hyprlang::INT>("plugin:magic-mouse-gaps:margin");
    static auto       PSIZE   = CConfigValue<Hyprlang::INT>("plugin:magic-mouse-gaps:size");
    static auto       PCLASS  = CConfigValue<std::string>("plugin:magic-mouse-gaps:class");
    static auto       PEDGE   = CConfigValue<std::string>("plugin:magic-mouse-gaps:edge");

    Vector2D          newCoords = local;
    Vector2D          surfacePos;
    Vector2D          surfaceCoords;
    bool              foundSurface = false;
    SP<CLayerSurface> pFoundLayerSurface;
    const auto        PMONITOR = g_pCompositor->getMonitorFromCursor();

    if (std::string{*PCLASS} != STRVAL_EMPTY && !(*PCLASS).empty() && *PSIZE > 0 && PMONITOR && PMONITOR == g_pCompositor->m_lastMonitor && g_pCompositor->m_lastWindow &&
        !g_pCompositor->m_lastWindow->m_class.empty()) {
        if (!foundSurface && !g_pCompositor->m_lastWindow.expired() && RE2::FullMatch(g_pCompositor->m_lastWindow->m_class, *PCLASS)) {
            const auto& winSize = g_pCompositor->m_lastWindow->m_realSize->goal();
            if ((*PEDGE).contains("l") && local.x < 0 && local.x >= -*PSIZE)
                newCoords.x = *PMARGIN;
            else if ((*PEDGE).contains("r") && local.x > winSize.x && local.x <= winSize.x + *PSIZE)
                newCoords.x = winSize.x - *PMARGIN;
            if ((*PEDGE).contains("t") && local.y < 0 && local.y >= -*PSIZE)
                newCoords.y = *PMARGIN;
            else if ((*PEDGE).contains("b") && local.y > winSize.y && local.y <= winSize.y + *PSIZE)
                newCoords.y = winSize.y - *PMARGIN;
        }
    }

    (*(origMotion)g_pMouseMotionHook->m_original)(thisptr, time_msec, newCoords);
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:magic-mouse-gaps:margin", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:magic-mouse-gaps:size", Hyprlang::INT{32});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:magic-mouse-gaps:class", Hyprlang::STRING{"firefox"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:magic-mouse-gaps:edge", Hyprlang::STRING{"t"});

    auto FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "sendPointerMotion");
    for (auto& fn : FNS) {
        if (!fn.demangled.contains("CSeatManager"))
            continue;

        g_pMouseMotionHook = HyprlandAPI::createFunctionHook(PHANDLE, fn.address, (void*)::hkNotifyMotion);
        break;
    }

    bool success = g_pMouseMotionHook;
    if (!success) {
        HyprlandAPI::addNotification(PHANDLE, "[magic-mouse-gaps] Hook init failed", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[magic-mouse-gaps] Hook init failed");
    }

    success = success && g_pMouseMotionHook->hook();

    if (!success) {
        HyprlandAPI::addNotification(PHANDLE, "[magic-mouse-gaps] Hook failed", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[magic-mouse-gaps] Hook failed");
    }

    return {"magic-mouse-gaps",
            "A plugin to move mouse events from gaps to nearby matching window "
            "(use firefox tabs from gaps)",
            "Dregu", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    ;
}
