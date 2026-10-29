// Flipeo — the plugin browser ("package manager" view).
//
// It scans installed Flipper application packages (.fap) under /ext/apps and
// lists them grouped by their category folder, Sileo-style. Selecting an entry
// shows its details and offers to launch it through the system loader.
#include "jailflip_i.h"
#include <loader/loader.h>

#define APPS_ROOT "/ext/apps"
#define MAX_PACKAGES 128

// Store the launch path in the package's "description" slot so the detail
// screen can both display and act on it without a second lookup.
static void jailflip_package_set(
    JailFlipPackage* pkg,
    const char* name,
    const char* category,
    const char* path) {
    pkg->name = furi_string_alloc_set(name);
    pkg->category = furi_string_alloc_set(category);
    pkg->description = furi_string_alloc_set(path);
}

void jailflip_store_free(JailFlipApp* app) {
    if(!app->packages) return;
    for(size_t i = 0; i < app->package_count; i++) {
        furi_string_free(app->packages[i].name);
        furi_string_free(app->packages[i].category);
        furi_string_free(app->packages[i].description);
    }
    free(app->packages);
    app->packages = NULL;
    app->package_count = 0;
}

// Scan APPS_ROOT/<category>/*.fap into app->packages.
void jailflip_store_load(JailFlipApp* app) {
    app->packages = malloc(sizeof(JailFlipPackage) * MAX_PACKAGES);
    app->package_count = 0;

    Storage* storage = app->storage;
    File* cat_dir = storage_file_alloc(storage);
    FileInfo cat_info;
    char cat_name[128];

    if(!storage_dir_open(cat_dir, APPS_ROOT)) {
        storage_dir_close(cat_dir);
        storage_file_free(cat_dir);
        return;
    }

    FuriString* path = furi_string_alloc();

    while(storage_dir_read(cat_dir, &cat_info, cat_name, sizeof(cat_name))) {
        if(!(cat_info.flags & FSF_DIRECTORY)) continue; // only category folders

        furi_string_printf(path, APPS_ROOT "/%s", cat_name);

        File* app_dir = storage_file_alloc(storage);
        FileInfo app_info;
        char app_name[128];

        if(storage_dir_open(app_dir, furi_string_get_cstr(path))) {
            while(storage_dir_read(app_dir, &app_info, app_name, sizeof(app_name))) {
                if(app_info.flags & FSF_DIRECTORY) continue;

                size_t len = strlen(app_name);
                if(len < 5 || strcmp(app_name + len - 4, ".fap") != 0) continue;
                if(app->package_count >= MAX_PACKAGES) break;

                // Strip the .fap suffix for a friendlier display name.
                char display[128];
                strlcpy(display, app_name, sizeof(display));
                display[len - 4] = '\0';

                FuriString* full = furi_string_alloc_printf(
                    APPS_ROOT "/%s/%s", cat_name, app_name);

                jailflip_package_set(
                    &app->packages[app->package_count],
                    display,
                    cat_name,
                    furi_string_get_cstr(full));
                app->package_count++;

                furi_string_free(full);
            }
        }
        storage_dir_close(app_dir);
        storage_file_free(app_dir);
    }

    furi_string_free(path);
    storage_dir_close(cat_dir);
    storage_file_free(cat_dir);

    FURI_LOG_I(JAILFLIP_TAG, "Flipeo found %zu plugins", app->package_count);
}

static void jailflip_store_item_callback(void* context, uint32_t index) {
    JailFlipApp* app = context;
    jailflip_store_show_detail(app, index);
}

void jailflip_store_build_menu(JailFlipApp* app) {
    submenu_reset(app->store);
    submenu_set_header(app->store, "Flipeo");

    if(app->package_count == 0) {
        // Nothing installed — leave a single informational row.
        submenu_add_item(
            app->store, "No .fap plugins found", 0, jailflip_store_item_callback, app);
        return;
    }

    for(size_t i = 0; i < app->package_count; i++) {
        submenu_add_item(
            app->store,
            furi_string_get_cstr(app->packages[i].name),
            i,
            jailflip_store_item_callback,
            app);
    }
}

// Launch the selected .fap through the system loader.
static void jailflip_store_launch(JailFlipApp* app, const char* fap_path) {
    Loader* loader = furi_record_open(RECORD_LOADER);
    LoaderStatus status = loader_start(loader, FAP_LOADER_APP_NAME, fap_path, NULL);
    if(status != LoaderStatusOk) {
        FURI_LOG_E(JAILFLIP_TAG, "launch failed (%d): %s", status, fap_path);
    }
    furi_record_close(RECORD_LOADER);
}

// Keep track of which package the detail screen is showing.
static uint32_t s_detail_index;

static void jailflip_detail_launch(GuiButtonType result, InputType type, void* context) {
    JailFlipApp* app = context;
    UNUSED(result);
    if(type != InputTypeShort) return;
    if(s_detail_index >= app->package_count) return;
    jailflip_store_launch(app, furi_string_get_cstr(app->packages[s_detail_index].description));
}

void jailflip_store_show_detail(JailFlipApp* app, uint32_t index) {
    widget_reset(app->detail);

    if(index >= app->package_count) {
        widget_add_text_scroll_element(
            app->detail, 0, 0, 128, 64,
            "No plugins installed.\n\nPut .fap files under\n/ext/apps/<Category>/");
        view_dispatcher_switch_to_view(app->view_dispatcher, JailFlipViewDetail);
        return;
    }

    s_detail_index = index;
    JailFlipPackage* pkg = &app->packages[index];

    FuriString* body = furi_string_alloc_printf(
        "\e#%s\e#\nCategory: %s\n\nPath:\n%s",
        furi_string_get_cstr(pkg->name),
        furi_string_get_cstr(pkg->category),
        furi_string_get_cstr(pkg->description));

    widget_add_text_scroll_element(app->detail, 0, 0, 128, 52, furi_string_get_cstr(body));
    widget_add_button_element(
        app->detail, GuiButtonTypeCenter, "Launch", jailflip_detail_launch, app);

    furi_string_free(body);
    view_dispatcher_switch_to_view(app->view_dispatcher, JailFlipViewDetail);
}
