// JailFlip — entry point, app lifecycle and main menu.
#include "jailflip_i.h"

// ---- main menu indices -----------------------------------------------------
typedef enum {
    MenuIndexStore,
    MenuIndexBootLogo,
    MenuIndexKeys,
    MenuIndexAbout,
} MenuIndex;

// Forward declarations of callbacks.
static void jailflip_menu_callback(void* context, uint32_t index);
static uint32_t jailflip_exit_callback(void* context);
static uint32_t jailflip_back_to_menu_callback(void* context);

// ---- about screen ----------------------------------------------------------
static void jailflip_show_about(JailFlipApp* app) {
    widget_reset(app->detail);
    widget_add_text_scroll_element(
        app->detail,
        0,
        0,
        128,
        64,
        "\e#JailFlip\e#\n"
        "A mod hub for Flipper Zero.\n\n"
        "Flipeo: browse and launch\n"
        "installed plugins (.fap).\n\n"
        "Boot Logo: set a 128x64\n"
        "splash from the SD card.\n\n"
        "Keys: browse stored SubGHz,\n"
        "NFC, iButton and RFID keys.\n\n"
        "All of this is user-space:\n"
        "nothing is unlocked that the\n"
        "open firmware didn't already\n"
        "allow. v0.1");
    view_dispatcher_switch_to_view(app->view_dispatcher, JailFlipViewDetail);
}

// ---- menu wiring -----------------------------------------------------------
static void jailflip_build_main_menu(JailFlipApp* app) {
    submenu_reset(app->menu);
    submenu_set_header(app->menu, "JailFlip");
    submenu_add_item(app->menu, "Flipeo (plugins)", MenuIndexStore, jailflip_menu_callback, app);
    submenu_add_item(app->menu, "Boot Logo", MenuIndexBootLogo, jailflip_menu_callback, app);
    submenu_add_item(app->menu, "Keys", MenuIndexKeys, jailflip_menu_callback, app);
    submenu_add_item(app->menu, "About", MenuIndexAbout, jailflip_menu_callback, app);
}

static void jailflip_menu_callback(void* context, uint32_t index) {
    JailFlipApp* app = context;
    switch(index) {
    case MenuIndexStore:
        jailflip_store_build_menu(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, JailFlipViewStore);
        break;
    case MenuIndexBootLogo:
        jailflip_logo_pick(app);
        break;
    case MenuIndexKeys:
        jailflip_keys_build_menu(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, JailFlipViewKeys);
        break;
    case MenuIndexAbout:
        jailflip_show_about(app);
        break;
    default:
        break;
    }
}

// Back from a sub-view returns to the main menu.
static uint32_t jailflip_back_to_menu_callback(void* context) {
    UNUSED(context);
    return JailFlipViewMenu;
}

// Back from the main menu exits the app.
static uint32_t jailflip_exit_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

// ---- lifecycle -------------------------------------------------------------
static JailFlipApp* jailflip_alloc(void) {
    JailFlipApp* app = malloc(sizeof(JailFlipApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    // Make sure our data dir exists so later writes/reads don't surprise us.
    storage_common_mkdir(app->storage, JAILFLIP_DATA_DIR);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->menu = submenu_alloc();
    app->store = submenu_alloc();
    app->keys = submenu_alloc();
    app->detail = widget_alloc();

    // Register views.
    view_dispatcher_add_view(
        app->view_dispatcher, JailFlipViewMenu, submenu_get_view(app->menu));
    view_dispatcher_add_view(
        app->view_dispatcher, JailFlipViewStore, submenu_get_view(app->store));
    view_dispatcher_add_view(
        app->view_dispatcher, JailFlipViewKeys, submenu_get_view(app->keys));
    view_dispatcher_add_view(
        app->view_dispatcher, JailFlipViewDetail, widget_get_view(app->detail));

    // Back-button navigation.
    view_set_previous_callback(submenu_get_view(app->menu), jailflip_exit_callback);
    view_set_previous_callback(submenu_get_view(app->store), jailflip_back_to_menu_callback);
    view_set_previous_callback(submenu_get_view(app->keys), jailflip_back_to_menu_callback);
    view_set_previous_callback(widget_get_view(app->detail), jailflip_back_to_menu_callback);

    jailflip_store_load(app);
    jailflip_build_main_menu(app);

    return app;
}

static void jailflip_free(JailFlipApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, JailFlipViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, JailFlipViewStore);
    view_dispatcher_remove_view(app->view_dispatcher, JailFlipViewKeys);
    view_dispatcher_remove_view(app->view_dispatcher, JailFlipViewDetail);

    submenu_free(app->menu);
    submenu_free(app->store);
    submenu_free(app->keys);
    widget_free(app->detail);

    view_dispatcher_free(app->view_dispatcher);
    jailflip_store_free(app);

    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    free(app);
}

// ---- entry point -----------------------------------------------------------
int32_t jailflip_app(void* p) {
    UNUSED(p);
    JailFlipApp* app = jailflip_alloc();

    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, JailFlipViewMenu);

    view_dispatcher_run(app->view_dispatcher);

    jailflip_free(app);
    return 0;
}
