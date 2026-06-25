#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QCoreApplication>
#include <QFileInfo>
#include <QMainWindow>
#include <QString>
#ifdef _WIN32
#include <windows.h>
#endif

#include "ChatBot.hpp"
#include "ui/ChatDock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("RoninOBSChat", "en-US")

static ChatBot  *g_bot  = nullptr;
static ChatDock *g_dock = nullptr;

static void on_frontend_event(enum obs_frontend_event event, void *)
{
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
        auto *mainWin = static_cast<QMainWindow *>(obs_frontend_get_main_window());
        g_dock = new ChatDock(g_bot, mainWin);
        obs_frontend_add_custom_qdock("ronin-obs-chat", g_dock);

    } else if (event == OBS_FRONTEND_EVENT_EXIT) {
        if (g_bot) g_bot->saveConfig();
    }
}

bool obs_module_load(void)
{
#ifdef _WIN32
    // Register our DLL's directory with Qt's plugin search path so Qt can
    // find the bundled TLS backend in the tls/ subdirectory next to our DLL.
    HMODULE hmod = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&obs_module_load),
                           &hmod)) {
        wchar_t path[MAX_PATH] = {};
        if (GetModuleFileNameW(hmod, path, MAX_PATH)) {
            QCoreApplication::addLibraryPath(
                QFileInfo(QString::fromWCharArray(path)).absolutePath());
        }
    }
#endif

    blog(LOG_INFO, "[RoninOBSChat] Loading plugin v%s", PLUGIN_VERSION);

    g_bot = new ChatBot();
    g_bot->loadConfig();

    obs_frontend_add_event_callback(on_frontend_event, nullptr);

    return true;
}

void obs_module_unload(void)
{
    obs_frontend_remove_event_callback(on_frontend_event, nullptr);

    // Dock is owned by OBS's main window — don't delete it manually.
    // ChatBot cleanup:
    delete g_bot;
    g_bot  = nullptr;
    g_dock = nullptr;

    blog(LOG_INFO, "[RoninOBSChat] Plugin unloaded");
}

const char *obs_module_description(void)
{
    return "YouTube live chat bot with commands, timed messages, and AutoMod.";
}

const char *obs_module_author(void)
{
    return "Ronin";
}
