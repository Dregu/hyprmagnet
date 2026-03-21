
#include <hyprland/src/helpers/math/Direction.hpp>
#include <unistd.h>
#include <string>

#include <hyprutils/math/Vector2D.hpp>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/PointerManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/config/legacy/ConfigManager.hpp>
#include <hyprland/src/config/shared/workspace/WorkspaceRuleManager.hpp>
#include <hyprland/src/layout/target/Target.hpp>
#include <hyprland/src/helpers/Monitor.hpp>

inline HANDLE         PHANDLE = nullptr;

inline CFunctionHook* g_pMouseHook = nullptr;
typedef void (*origMotion)(CInputManager*, uint32_t, bool, bool, std::optional<Vector2D>);

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

CTimer     g_warped;

PHLMONITOR getMonitorAcrossGap(Math::eDirection dir, bool inter) {
    const auto ACTIVE   = Desktop::focusState()->monitor();
    const auto COORDS   = g_pPointerManager->position();
    float      bestDist = __FLT_MAX__;
    PHLMONITOR bestMon;
    for (auto& mon : g_pCompositor->m_monitors) {
        const auto POINT = mon->logicalBox().closestPoint(COORDS);
        const auto DIST  = (dir == Math::DIRECTION_LEFT || dir == Math::DIRECTION_RIGHT) ? abs(COORDS.y - POINT.y) : abs(COORDS.x - POINT.x);
        if (DIST < bestDist) {
            bool INTERSECTV = !((mon->m_position.y + mon->m_size.y < ACTIVE->m_position.y) || (ACTIVE->m_position.y + ACTIVE->m_size.y < mon->m_position.y));
            bool INTERSECTH = !((mon->m_position.x + mon->m_size.x < ACTIVE->m_position.x) || (ACTIVE->m_position.x + ACTIVE->m_size.x < mon->m_position.x));
            if (!inter) {
                INTERSECTV = INTERSECTV && COORDS.y > mon->m_position.y && COORDS.y < mon->m_position.y + mon->m_size.y;
                INTERSECTH = INTERSECTH && COORDS.x > mon->m_position.x && COORDS.x < mon->m_position.x + mon->m_size.x;
            }
            if ((dir == Math::DIRECTION_LEFT && mon->logicalBox().middle().x < COORDS.x && INTERSECTV) ||
                (dir == Math::DIRECTION_RIGHT && mon->logicalBox().middle().x > COORDS.x && INTERSECTV) ||
                (dir == Math::DIRECTION_UP && mon->logicalBox().middle().y < COORDS.y && INTERSECTH) ||
                (dir == Math::DIRECTION_DOWN && mon->logicalBox().middle().y > COORDS.y && INTERSECTH)) {
                bestDist = DIST;
                bestMon  = mon;
            }
        }
    }
    return bestMon;
}

void hkMouse(CInputManager* thisptr, uint32_t time, bool refocus, bool mouse, std::optional<Vector2D> overridePos) {
    static auto       PPAD   = HyprlandAPI::getConfigValue(PHANDLE, "plugin:magnet:pad");
    static auto       PDELAY = HyprlandAPI::getConfigValue(PHANDLE, "plugin:magnet:delay");
    static auto       PEDGE  = HyprlandAPI::getConfigValue(PHANDLE, "plugin:magnet:edge");
    static auto       PWARP  = HyprlandAPI::getConfigValue(PHANDLE, "plugin:magnet:warp");

    static auto       PBORDER  = HyprlandAPI::getConfigValue(PHANDLE, "general:border_size");
    static auto       PGAPSOUT = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_out");
    static auto       PHOTSPOT = HyprlandAPI::getConfigValue(PHANDLE, "cursor:hotspot_padding");

    const auto        PAD   = -sc<double>(std::any_cast<Hyprlang::INT>(PPAD->getValue()));
    const auto        DELAY = std::any_cast<Hyprlang::INT>(PDELAY->getValue());
    const std::string EDGE{std::any_cast<Hyprlang::STRING>(PEDGE->getValue())};
    const std::string WARP{std::any_cast<Hyprlang::STRING>(PWARP->getValue())};

    const auto        BORDER  = std::any_cast<Hyprlang::INT>(PBORDER->getValue());
    const auto*       GAPSOUT = sc<Config::CCssGapData*>(PGAPSOUT.ptr()->getData());
    const auto        HOTSPOT = std::any_cast<Hyprlang::INT>(PHOTSPOT->getValue());

    const auto        COORDS        = g_pPointerManager->position();
    const auto        COORDSFLOORED = COORDS.floor();
    const auto        MON           = Desktop::focusState()->monitor();
    const auto        WIN           = Desktop::focusState()->window();
    const auto        NEWWIN        = g_pCompositor->vectorToWindowUnified(COORDS, Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING);

    if (MON && g_warped.getMillis() > DELAY && !WARP.empty()) {
        const auto LOCAL  = COORDSFLOORED - MON->m_position;
        auto       BOX    = MON->logicalBox().expand(-HOTSPOT - 1);
        auto       MID    = BOX.middle();
        bool       BONKED = !BOX.containsPoint(COORDSFLOORED);
        if (BONKED) {
            Math::eDirection DIR{Math::DIRECTION_DEFAULT};
            if (BOX.containsPoint(COORDSFLOORED + Vector2D(1, 0)) && COORDS.x < MID.x)
                DIR = Math::DIRECTION_LEFT;
            else if (BOX.containsPoint(COORDSFLOORED + Vector2D(-1, 0)) && COORDS.x > MID.x)
                DIR = Math::DIRECTION_RIGHT;
            else if (BOX.containsPoint(COORDSFLOORED + Vector2D(0, 1)) && COORDS.y < MID.y)
                DIR = Math::DIRECTION_UP;
            else if (BOX.containsPoint(COORDSFLOORED + Vector2D(0, -1)) && COORDS.y > MID.y)
                DIR = Math::DIRECTION_DOWN;
            if (DIR != Math::DIRECTION_DEFAULT) {
                const bool INTER = WARP.contains('i');
                for (char c : WARP) {
                    if (c == 'i') {
                        if (auto NEWMON = g_pCompositor->getMonitorInDirection(DIR); NEWMON) {
                            auto NEWPOS = NEWMON->logicalBox().closestPoint(COORDS);
                            Log::logger->log(Log::DEBUG, "[hyprmagnet] INT WARP TO {} {}", NEWMON->m_name, NEWPOS);
                            g_warped.reset();
                            g_pPointerManager->warpTo(NEWPOS);
                            return;
                        }
                    } else if (c == 'g') {
                        if (auto NEWMON = getMonitorAcrossGap(DIR, INTER); NEWMON) {
                            auto NEWPOS = NEWMON->logicalBox().closestPoint(COORDS);
                            Log::logger->log(Log::DEBUG, "[hyprmagnet] GAP WARP TO {} {}", NEWMON->m_name, NEWPOS);
                            g_warped.reset();
                            g_pPointerManager->warpTo(NEWPOS);
                            return;
                        }
                    }
                }
            }
        }
    }

    if (g_pInputManager->m_lastFocusOnLS) {
        (*(origMotion)g_pMouseHook->m_original)(thisptr, time, refocus, mouse, overridePos);
        return;
    }

    if (WIN && (!NEWWIN || WIN == NEWWIN) && WIN->m_ruleApplicator->m_tagKeeper.isTagged("magnet")) {
        const auto WSRULE = Config::workspaceRuleMgr()->getWorkspaceRuleFor(WIN->layoutTarget()->workspace()).value_or(Config::CWorkspaceRule{});
        auto       GAPS   = WSRULE.m_gapsOut.value_or(*GAPSOUT);
        auto       BOX    = WIN->getWindowMainSurfaceBox();
        auto       GAPBOX = BOX.copy();
        auto       LEFT   = EDGE.contains('l');
        auto       RIGHT  = EDGE.contains('r');
        auto       TOP    = EDGE.contains('t');
        auto       BOTTOM = EDGE.contains('b');
        GAPBOX.addExtents(SBoxExtents{
            Vector2D{LEFT ? (int)GAPS.m_left : 0, TOP ? (int)GAPS.m_top : 0},
            Vector2D{RIGHT ? (int)GAPS.m_right : 0, BOTTOM ? (int)GAPS.m_bottom : 0},
        });
        GAPBOX.expand(BORDER);
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
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:magnet:delay", Hyprlang::INT{500});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:magnet:edge", Hyprlang::STRING{"t"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:magnet:warp", Hyprlang::STRING{""});

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

    return {"hyprmagnet", "Move mouse events from gaps to nearby windows and warp between monitors", "Dregu", "0.3.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    ;
}
