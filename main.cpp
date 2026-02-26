
#include <unistd.h>
#include <string>

#include <hyprutils/math/Vector2D.hpp>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/PointerManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/layout/target/Target.hpp>

inline HANDLE         PHANDLE = nullptr;

inline CFunctionHook* g_pMouseHook = nullptr;
typedef void (*origMotion)(CInputManager*, uint32_t, bool, bool, std::optional<Vector2D>);

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

void hkMouse(CInputManager* thisptr, uint32_t time, bool refocus, bool mouse, std::optional<Vector2D> overridePos) {
    if (g_pInputManager->m_lastFocusOnLS) {
        (*(origMotion)g_pMouseHook->m_original)(thisptr, time, refocus, mouse, overridePos);
        return;
    }
    static auto PEDGE    = CConfigValue<std::string>("plugin:magnet:edge");
    static auto PPAD     = CConfigValue<Hyprlang::INT>("plugin:magnet:pad");
    static auto PGAPSOUT = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_out");
    static auto PBORDER  = CConfigValue<Hyprlang::INT>("general:border_size");

    const auto* GAPSOUT = sc<CCssGapData*>(PGAPSOUT.ptr()->getData());
    const auto  PAD     = -sc<double>(*PPAD);
    const auto  COORDS  = g_pPointerManager->position();
    const auto  WIN     = Desktop::focusState()->window();
    const auto  NEWWIN  = g_pCompositor->vectorToWindowUnified(COORDS, Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING);

    if (WIN && (!NEWWIN || WIN == NEWWIN) && WIN->m_ruleApplicator->m_tagKeeper.isTagged("magnet")) {
        const auto WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(WIN->layoutTarget()->workspace());
        auto       GAPS          = WORKSPACERULE.gapsOut.value_or(*GAPSOUT);
        auto       BOX           = WIN->getWindowMainSurfaceBox();
        auto       GAPBOX        = BOX.copy();
        auto       LEFT          = (*PEDGE).contains("l");
        auto       RIGHT         = (*PEDGE).contains("r");
        auto       TOP           = (*PEDGE).contains("t");
        auto       BOTTOM        = (*PEDGE).contains("b");
        GAPBOX.addExtents(SBoxExtents{
            Vector2D{LEFT ? (int)GAPS.m_left : 0, TOP ? (int)GAPS.m_top : 0},
            Vector2D{RIGHT ? (int)GAPS.m_right : 0, BOTTOM ? (int)GAPS.m_bottom : 0},
        });
        GAPBOX.expand(*PBORDER);
        if (GAPBOX.containsPoint(COORDS) &&
            ((LEFT && COORDS.x <= BOX.x) || (RIGHT && COORDS.x >= BOX.x + BOX.w) || (TOP && COORDS.y <= BOX.y) || (BOTTOM && COORDS.y >= BOX.y + BOX.h))) {
            (*(origMotion)g_pMouseHook->m_original)(thisptr, time, refocus, mouse, BOX.expand(PAD).closestPoint(COORDS));
            return;
        }
    }
    (*(origMotion)g_pMouseHook->m_original)(thisptr, time, refocus, mouse, overridePos);
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string COMPOSITOR_HASH = __hyprland_api_get_hash();
    const std::string CLIENT_HASH     = __hyprland_api_get_client_hash();

    if (COMPOSITOR_HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprmagnet] Mismatched headers! Can't proceed.", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hyprmagnet] Version mismatch");
    }

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:magnet:pad", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:magnet:edge", Hyprlang::STRING{"t"});

    auto FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "mouseMoveUnified");
    for (auto& fn : FNS) {
        if (!fn.demangled.contains("CInputManager"))
            continue;

        g_pMouseHook = HyprlandAPI::createFunctionHook(PHANDLE, fn.address, (void*)::hkMouse);
        break;
    }

    bool success = g_pMouseHook;
    if (!success) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprmagnet] Hook init failed", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hyprmagnet] Hook init failed");
    }

    success = success && g_pMouseHook->hook();

    if (!success) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprmagnet] Hook failed", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hyprmagnet] Hook failed");
    }

    return {"hyprmagnet", "Move mouse events from gaps to nearby windows", "Dregu", "0.2.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    ;
}
