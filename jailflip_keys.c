// Keys — browse key/dump files already stored on the SD card.
//
// This is a read-only convenience browser over the standard Flipper data
// folders. It opens the system file browser scoped to the chosen folder so you
// can see what you have saved; it does not transmit, emulate or modify keys.
#include "jailflip_i.h"

typedef struct {
    const char* label;
    const char* folder;
    const char* ext;
} KeyCategory;

static const KeyCategory key_categories[] = {
    {"SubGHz", JAILFLIP_SUBGHZ_DIR, ".sub"},
    {"NFC", JAILFLIP_NFC_DIR, ".nfc"},
    {"iButton", JAILFLIP_IBUTTON_DIR, ".ibtn"},
    {"LF RFID", JAILFLIP_LFRFID_DIR, ".rfid"},
};
static const size_t key_category_count = COUNT_OF(key_categories);

static void jailflip_keys_item_callback(void* context, uint32_t index) {
    JailFlipApp* app = context;
    jailflip_keys_show_category(app, index);
}

void jailflip_keys_build_menu(JailFlipApp* app) {
    submenu_reset(app->keys);
    submenu_set_header(app->keys, "Keys");
    for(size_t i = 0; i < key_category_count; i++) {
        submenu_add_item(
            app->keys, key_categories[i].label, i, jailflip_keys_item_callback, app);
    }
}

void jailflip_keys_show_category(JailFlipApp* app, uint32_t index) {
    if(index >= key_category_count) return;
    const KeyCategory* cat = &key_categories[index];

    // Make sure the folder exists so the browser opens cleanly.
    storage_common_mkdir(app->storage, cat->folder);

    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, cat->ext, NULL);
    options.base_path = cat->folder;
    options.hide_ext = false;

    FuriString* selected = furi_string_alloc_set(cat->folder);
    // Read-only: we show the browser and discard the result. The user is just
    // inspecting what's stored; opening/emulating is the job of the dedicated
    // SubGHz/NFC/iButton apps, reachable via Flipeo.
    dialog_file_browser_show(app->dialogs, selected, selected, &options);
    furi_string_free(selected);

    // Return to the key menu after the browser closes.
    view_dispatcher_switch_to_view(app->view_dispatcher, JailFlipViewKeys);
}
