// Flipeo — the plugin browser ("package manager" view).
//
// Two sources of plugins:
//   * Installed  — scanned from /ext/apps/<Category>/*.fap (the main list).
//   * Available  — parsed from a bundled catalog file; reached via the top
//                  "＋ Add plugin" row. Selecting one shows its details and,
//                  if its .fap is present, launches it.
//
// All user-space: it lists files and launches already-installed apps.
#include "jailflip_i.h"
#include <loader/loader.h>

#define APPS_ROOT "/ext/apps"
#define MAX_PACKAGES 128
#define MAX_AVAILABLE 128

// Sentinel index for the "＋ Add plugin" row (real packages use 0..n-1).
#define FLIPEO_ADD_ITEM 0xFFFFFFFFUL

// Path of the plugin the detail screen can launch (borrowed, may be NULL).
static FuriString* s_launch_path;

// --- package helpers --------------------------------------------------------
static void jailflip_package_set(
    JailFlipPackage* pkg,
    const char* name,
    const char* category,
    const char* path,
    const char* description) {
    pkg->name = furi_string_alloc_set(name);
    pkg->category = furi_string_alloc_set(category);
    // We keep the launch path AND a human description; pack path first line,
    // description second, separated so the detail screen can split them.
    pkg->description = furi_string_alloc();
    furi_string_printf(pkg->description, "%s\n%s", path, description);
}

static const char* jailflip_pkg_path(const JailFlipPackage* pkg) {
    // Path is the text before the first newline in description.
    static char buf[256];
    const char* full = furi_string_get_cstr(pkg->description);
    const char* nl = strchr(full, '\n');
    size_t len = nl ? (size_t)(nl - full) : strlen(full);
    if(len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, full, len);
    buf[len] = '\0';
    return buf;
}

static const char* jailflip_pkg_desc(const JailFlipPackage* pkg) {
    const char* full = furi_string_get_cstr(pkg->description);
    const char* nl = strchr(full, '\n');
    return nl ? nl + 1 : "";
}

static void jailflip_packages_free(JailFlipPackage* arr, size_t count) {
    if(!arr) return;
    for(size_t i = 0; i < count; i++) {
        furi_string_free(arr[i].name);
        furi_string_free(arr[i].category);
        furi_string_free(arr[i].description);
    }
    free(arr);
}

void jailflip_store_free(JailFlipApp* app) {
    jailflip_packages_free(app->packages, app->package_count);
    app->packages = NULL;
    app->package_count = 0;
    jailflip_packages_free(app->available, app->available_count);
    app->available = NULL;
    app->available_count = 0;
}

// --- scan installed .fap ----------------------------------------------------
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
        if(!(cat_info.flags & FSF_DIRECTORY)) continue;
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

                char display[128];
                strlcpy(display, app_name, sizeof(display));
                display[len - 4] = '\0';

                FuriString* full = furi_string_alloc_printf(
                    APPS_ROOT "/%s/%s", cat_name, app_name);
                jailflip_package_set(
                    &app->packages[app->package_count],
                    display, cat_name, furi_string_get_cstr(full), "Installed plugin.");
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
    FURI_LOG_I(JAILFLIP_TAG, "Flipeo: %zu installed plugins", app->package_count);
}

// --- parse bundled catalog --------------------------------------------------
// Catalog line format:  name | category | path | description
// Blank lines and lines starting with '#' are ignored.
static char* jailflip_read_file(Storage* storage, const char* path, size_t* out_len) {
    File* f = storage_file_alloc(storage);
    char* buf = NULL;
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(f);
        if(size > 0 && size < 64 * 1024) {
            buf = malloc(size + 1);
            uint16_t rd = storage_file_read(f, buf, size);
            buf[rd] = '\0';
            if(out_len) *out_len = rd;
        }
    }
    storage_file_close(f);
    storage_file_free(f);
    return buf;
}

static void jailflip_trim(char* s) {
    char* p = s;
    while(*p == ' ' || *p == '\t') p++;
    if(p != s) memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while(n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) s[--n] = '\0';
}

void jailflip_catalog_load(JailFlipApp* app) {
    app->available = malloc(sizeof(JailFlipPackage) * MAX_AVAILABLE);
    app->available_count = 0;

    char* buf = jailflip_read_file(app->storage, JAILFLIP_CATALOG, NULL);
    if(!buf) {
        FURI_LOG_W(JAILFLIP_TAG, "no catalog at %s", JAILFLIP_CATALOG);
        return;
    }

    char* save = NULL;
    for(char* line = strtok_r(buf, "\n", &save); line;
        line = strtok_r(NULL, "\n", &save)) {
        jailflip_trim(line);
        if(line[0] == '\0' || line[0] == '#') continue;
        if(app->available_count >= MAX_AVAILABLE) break;

        // Split on '|' into up to 4 fields.
        char* fields[4] = {line, "", "", ""};
        int nf = 1;
        for(char* c = line; *c && nf < 4; c++) {
            if(*c == '|') {
                *c = '\0';
                fields[nf++] = c + 1;
            }
        }
        for(int i = 0; i < 4; i++) jailflip_trim(fields[i]);
        if(fields[0][0] == '\0') continue;

        jailflip_package_set(
            &app->available[app->available_count],
            fields[0], fields[1], fields[2], fields[3]);
        app->available_count++;
    }
    free(buf);
    FURI_LOG_I(JAILFLIP_TAG, "Flipeo: %zu catalog plugins", app->available_count);
}

// --- launch -----------------------------------------------------------------
static void jailflip_store_launch(JailFlipApp* app, const char* fap_path) {
    Loader* loader = furi_record_open(RECORD_LOADER);
    LoaderStatus status = loader_start(loader, FAP_LOADER_APP_NAME, fap_path, NULL);
    if(status != LoaderStatusOk) {
        FURI_LOG_E(JAILFLIP_TAG, "launch failed (%d): %s", status, fap_path);
    }
    furi_record_close(RECORD_LOADER);
}

static void jailflip_detail_launch(GuiButtonType result, InputType type, void* context) {
    JailFlipApp* app = context;
    UNUSED(result);
    if(type != InputTypeShort) return;
    if(s_launch_path && !furi_string_empty(s_launch_path)) {
        jailflip_store_launch(app, furi_string_get_cstr(s_launch_path));
    }
}

// --- installed list (main Flipeo view) --------------------------------------
static void jailflip_store_item_callback(void* context, uint32_t index) {
    JailFlipApp* app = context;
    if(index == FLIPEO_ADD_ITEM) {
        jailflip_store_open_add(app);
    } else {
        jailflip_store_show_detail(app, index);
    }
}

void jailflip_store_build_menu(JailFlipApp* app) {
    submenu_reset(app->store);
    submenu_set_header(app->store, "Flipeo");

    // Top "+" row: open the catalog of available plugins.
    submenu_add_item(
        app->store, "+ Add plugin", FLIPEO_ADD_ITEM, jailflip_store_item_callback, app);

    for(size_t i = 0; i < app->package_count; i++) {
        submenu_add_item(
            app->store,
            furi_string_get_cstr(app->packages[i].name),
            i,
            jailflip_store_item_callback,
            app);
    }
}

void jailflip_store_show_detail(JailFlipApp* app, uint32_t index) {
    widget_reset(app->detail);
    if(index >= app->package_count) {
        s_launch_path = NULL;
        widget_add_text_scroll_element(
            app->detail, 0, 0, 128, 64,
            "No plugins installed.\n\nPut .fap files under\n/ext/apps/<Category>/");
        view_dispatcher_switch_to_view(app->view_dispatcher, JailFlipViewDetail);
        return;
    }

    JailFlipPackage* pkg = &app->packages[index];
    // description packs "path\ndescription"; the loader needs only the path.
    static FuriString* clean;
    if(!clean) clean = furi_string_alloc();
    furi_string_set(clean, jailflip_pkg_path(pkg));
    s_launch_path = clean;

    FuriString* body = furi_string_alloc_printf(
        "\e#%s\e#\nCategory: %s\n\n%s\n\nPath:\n%s",
        furi_string_get_cstr(pkg->name),
        furi_string_get_cstr(pkg->category),
        jailflip_pkg_desc(pkg),
        jailflip_pkg_path(pkg));

    widget_add_text_scroll_element(app->detail, 0, 0, 128, 52, furi_string_get_cstr(body));
    widget_add_button_element(
        app->detail, GuiButtonTypeCenter, "Launch", jailflip_detail_launch, app);

    furi_string_free(body);
    view_dispatcher_switch_to_view(app->view_dispatcher, JailFlipViewDetail);
}

// --- available list (the "+" view) ------------------------------------------
static void jailflip_add_item_callback(void* context, uint32_t index) {
    JailFlipApp* app = context;
    jailflip_store_show_available(app, index);
}

void jailflip_store_open_add(JailFlipApp* app) {
    submenu_reset(app->add);
    submenu_set_header(app->add, "Add plugin");

    if(app->available_count == 0) {
        submenu_add_item(
            app->add, "Catalog empty / missing", 0, jailflip_add_item_callback, app);
    } else {
        for(size_t i = 0; i < app->available_count; i++) {
            submenu_add_item(
                app->add,
                furi_string_get_cstr(app->available[i].name),
                i,
                jailflip_add_item_callback,
                app);
        }
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, JailFlipViewAdd);
}

void jailflip_store_show_available(JailFlipApp* app, uint32_t index) {
    widget_reset(app->detail);
    if(index >= app->available_count) {
        s_launch_path = NULL;
        widget_add_text_scroll_element(
            app->detail, 0, 0, 128, 64,
            "No catalog entries.\n\nBundled file expected at:\n" JAILFLIP_CATALOG);
        view_dispatcher_switch_to_view(app->view_dispatcher, JailFlipViewDetail);
        return;
    }

    JailFlipPackage* pkg = &app->available[index];
    const char* path = jailflip_pkg_path(pkg);
    bool installed = storage_file_exists(app->storage, path);

    static FuriString* clean;
    if(!clean) clean = furi_string_alloc();
    furi_string_set(clean, path);
    s_launch_path = installed ? clean : NULL;

    FuriString* body = furi_string_alloc_printf(
        "\e#%s\e#\nCategory: %s\n\n%s\n\n%s\nPath:\n%s",
        furi_string_get_cstr(pkg->name),
        furi_string_get_cstr(pkg->category),
        jailflip_pkg_desc(pkg),
        installed ? "Status: installed" : "Status: NOT installed",
        path);

    widget_add_text_scroll_element(app->detail, 0, 0, 128, 52, furi_string_get_cstr(body));
    if(installed) {
        widget_add_button_element(
            app->detail, GuiButtonTypeCenter, "Launch", jailflip_detail_launch, app);
    }
    furi_string_free(body);
    view_dispatcher_switch_to_view(app->view_dispatcher, JailFlipViewDetail);
}
