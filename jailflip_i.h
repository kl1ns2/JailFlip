#pragma once

// JailFlip — internal shared definitions.
// A single-FAP "mod hub" for Flipper Zero:
//   * Flipeo  — plugin/package catalogue (Sileo-style browser)
//   * Boot Logo — pick a 128x64 splash image from the SD card
//   * Keys    — browse stored SubGHz / NFC / iButton / LF-RFID key files
//
// Everything here is ordinary user-space functionality: listing files on the
// SD card and launching other installed .fap apps. Nothing bypasses device
// security — the Flipper Zero firmware is already open.

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>

#define JAILFLIP_TAG "JailFlip"

// Where JailFlip keeps its own data on the SD card.
#define JAILFLIP_DATA_DIR  "/ext/apps_data/jailflip"
#define JAILFLIP_SPLASH    JAILFLIP_DATA_DIR "/splash.bmx"

// Folders scanned by the Keys browser.
#define JAILFLIP_SUBGHZ_DIR  "/ext/subghz"
#define JAILFLIP_NFC_DIR     "/ext/nfc"
#define JAILFLIP_IBUTTON_DIR "/ext/ibutton"
#define JAILFLIP_LFRFID_DIR  "/ext/lfrfid"

// View identifiers for the ViewDispatcher.
typedef enum {
    JailFlipViewMenu,     // main menu (submenu)
    JailFlipViewStore,    // Flipeo catalogue (submenu)
    JailFlipViewKeys,     // key category picker (submenu)
    JailFlipViewDetail,   // package / info detail (widget)
} JailFlipView;

// One entry in the Flipeo catalogue.
typedef struct {
    FuriString* name;
    FuriString* category;
    FuriString* description;
} JailFlipPackage;

typedef struct {
    Gui* gui;
    Storage* storage;
    DialogsApp* dialogs;
    ViewDispatcher* view_dispatcher;

    Submenu* menu;      // main menu
    Submenu* store;     // Flipeo list
    Submenu* keys;      // key categories
    Widget* detail;     // scrollable detail / about screen

    // Loaded catalogue (built-in defaults, or parsed from JAILFLIP_CATALOG).
    JailFlipPackage* packages;
    size_t package_count;
} JailFlipApp;

// jailflip_store.c
void jailflip_store_load(JailFlipApp* app);
void jailflip_store_free(JailFlipApp* app);
void jailflip_store_build_menu(JailFlipApp* app);
void jailflip_store_show_detail(JailFlipApp* app, uint32_t index);

// jailflip_keys.c
void jailflip_keys_build_menu(JailFlipApp* app);
void jailflip_keys_show_category(JailFlipApp* app, uint32_t index);

// jailflip_logo.c
void jailflip_logo_pick(JailFlipApp* app);
