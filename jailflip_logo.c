// Boot Logo — choose a 128x64 image to use as a splash.
//
// A FAP cannot rewrite the firmware's compiled-in boot screen. What it CAN do
// is stage an image in a known location; custom firmwares that support a
// user boot logo (e.g. Momentum) then read it at startup. We copy the chosen
// file to JAILFLIP_SPLASH and tell the user exactly what to do per firmware.
#include "jailflip_i.h"

void jailflip_logo_pick(JailFlipApp* app) {
    storage_common_mkdir(app->storage, JAILFLIP_DATA_DIR);

    // Flipper monochrome images are stored as .bm / .bmx. Let the user pick one.
    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, ".bmx", NULL);
    options.base_path = "/ext";
    options.hide_ext = false;

    FuriString* picked = furi_string_alloc_set("/ext");
    bool chosen = dialog_file_browser_show(app->dialogs, picked, picked, &options);

    widget_reset(app->detail);

    if(!chosen) {
        widget_add_text_scroll_element(
            app->detail, 0, 0, 128, 64,
            "Boot Logo\n\nNo image selected.\n\n"
            "Pick a 128x64 .bmx image\nto stage it as a splash.");
    } else {
        FuriString* err = furi_string_alloc();
        FS_Error res = storage_common_copy(
            app->storage, furi_string_get_cstr(picked), JAILFLIP_SPLASH);

        if(res == FSE_OK || res == FSE_EXIST) {
            furi_string_set(
                err,
                "\e#Boot Logo staged\e#\n"
                "Saved to:\n" JAILFLIP_SPLASH "\n\n"
                "To use it at boot, your\n"
                "custom firmware must support\n"
                "a user boot logo. On Momentum:\n"
                "Settings > Desktop > Boot\n"
                "Graphics, point it here.\n\n"
                "Stock firmware ignores this.");
        } else {
            furi_string_printf(err, "Copy failed (code %d).\nCheck SD card space.", res);
        }
        widget_add_text_scroll_element(
            app->detail, 0, 0, 128, 64, furi_string_get_cstr(err));
        furi_string_free(err);
    }

    furi_string_free(picked);
    view_dispatcher_switch_to_view(app->view_dispatcher, JailFlipViewDetail);
}
