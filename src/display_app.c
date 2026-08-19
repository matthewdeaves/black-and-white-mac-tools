/*
 * Black & White Display
 *
 * Sets the game's resolution, colour depth and fullscreen mode by editing the
 * XML registry the Mac port keeps its settings in. No binary patching.
 *
 * Carbon, so it runs on 10.3 through 10.6 on both PowerPC and Intel.
 */
#include <Carbon/Carbon.h>
#include "registry.h"

#define CMD_SAVE    'save'
#define CMD_CANCEL  'cncl'

#define MENU_RES    1000
#define MENU_DEPTH  1001

#define WIN_W  470
#define WIN_H  300

typedef struct { int w, h; } Res;

static reg_file   gReg;
static int        gHaveFile = 0;
static Res        gRes[64];
static int        gResCount = 0;
static ControlRef gResPop, gDepthPop, gFullChk, gDeskChk, gStatus;

/* ---- resolution list, from what the display actually supports ------------ */

static void addRes(int w, int h)
{
    int i;
    if (w < 640 || h < 480) return;
    for (i = 0; i < gResCount; i++)
        if (gRes[i].w == w && gRes[i].h == h) return;
    if (gResCount >= (int)(sizeof(gRes)/sizeof(gRes[0]))) return;
    gRes[gResCount].w = w; gRes[gResCount].h = h; gResCount++;
}

static int cmpRes(const void *a, const void *b)
{
    const Res *x = (const Res *)a, *y = (const Res *)b;
    if (x->w != y->w) return x->w - y->w;
    return x->h - y->h;
}

static void buildResList(int curW, int curH)
{
    CFArrayRef modes = CGDisplayAvailableModes(CGMainDisplayID());
    if (modes) {
        CFIndex n = CFArrayGetCount(modes), i;
        for (i = 0; i < n; i++) {
            CFDictionaryRef m = (CFDictionaryRef)CFArrayGetValueAtIndex(modes, i);
            CFNumberRef wn = (CFNumberRef)CFDictionaryGetValue(m, kCGDisplayWidth);
            CFNumberRef hn = (CFNumberRef)CFDictionaryGetValue(m, kCGDisplayHeight);
            int w = 0, h = 0;
            if (wn) CFNumberGetValue(wn, kCFNumberIntType, &w);
            if (hn) CFNumberGetValue(hn, kCFNumberIntType, &h);
            addRes(w, h);
        }
    }
    addRes(640,480); addRes(800,600); addRes(1024,768); addRes(1152,864);
    addRes(1280,960); addRes(1280,1024); addRes(1440,900); addRes(1600,1200);
    addRes(1680,1050); addRes(1920,1080); addRes(1920,1200);
    addRes(curW, curH);
    qsort(gRes, (size_t)gResCount, sizeof(Res), cmpRes);
}

/* ---- ui ------------------------------------------------------------------ */

static void setStatus(const char *s)
{
    CFStringRef c = CFStringCreateWithCString(NULL, s, kCFStringEncodingUTF8);
    if (!c) return;
    SetControlData(gStatus, kControlEntireControl, kControlStaticTextCFStringTag,
                   sizeof(c), &c);
    Draw1Control(gStatus);
    CFRelease(c);
}

static ControlRef label(WindowRef w, int l, int t, int width, int height, const char *s)
{
    Rect r; ControlRef c; CFStringRef cf;
    SetRect(&r, l, t, l + width, t + height);
    cf = CFStringCreateWithCString(NULL, s, kCFStringEncodingUTF8);
    CreateStaticTextControl(w, &r, cf, NULL, &c);
    if (cf) CFRelease(cf);
    return c;
}

static void doSave(void)
{
    int i, d, full, desk;
    char msg[512];

    if (!gHaveFile) { setStatus("Nothing to save: no settings file."); return; }

    i = GetControl32BitValue(gResPop) - 1;
    if (i < 0 || i >= gResCount) { setStatus("Pick a resolution first."); return; }
    d    = GetControl32BitValue(gDepthPop) == 2 ? 32 : 16;
    full = GetControl32BitValue(gFullChk) ? 1 : 0;
    desk = GetControl32BitValue(gDeskChk) ? 1 : 0;

    reg_set_int(&gReg, "ScreenW", gRes[i].w);
    reg_set_int(&gReg, "ScreenH", gRes[i].h);
    reg_set_int(&gReg, "ScreenD", d);
    reg_set_int(&gReg, "FullScreen", full);
    reg_set_int(&gReg, "UseDesktopRes", desk);
    reg_set_int(&gReg, "UseDesktopDepth", desk);

    if (reg_save(&gReg) == REG_OK) {
        snprintf(msg, sizeof(msg),
                 "Saved: %d x %d, %s, %s. The original settings are kept alongside.",
                 gRes[i].w, gRes[i].h,
                 d >= 32 ? "millions of colours" : "thousands of colours",
                 full ? "full screen" : "in a window");
        setStatus(msg);
    } else {
        setStatus("Could not write the settings file.");
    }
}

static OSStatus onCommand(EventHandlerCallRef h, EventRef e, void *ud)
{
    HICommand c;
    (void)h; (void)ud;
    if (GetEventParameter(e, kEventParamDirectObject, typeHICommand, NULL,
                          sizeof(c), NULL, &c) != noErr) return eventNotHandledErr;
    switch (c.commandID) {
    case CMD_SAVE:       doSave();                   return noErr;
    case CMD_CANCEL:
    case kHICommandQuit: QuitApplicationEventLoop(); return noErr;
    }
    return eventNotHandledErr;
}

int main(void)
{
    WindowRef win;
    ControlRef root, btn;
    Rect r;
    char path[1024], msg[512];
    EventTypeSpec spec = { kEventClassCommand, kEventProcessCommand };
    MenuRef rm, dm;
    int w = 800, h = 600, d = 16, full = 0, desk = 0, i, sel = 1;

    reg_default_path(path, sizeof(path));
    gHaveFile = (reg_load(path, &gReg) == REG_OK);
    if (gHaveFile) {
        reg_get_int(&gReg, "ScreenW", &w);
        reg_get_int(&gReg, "ScreenH", &h);
        reg_get_int(&gReg, "ScreenD", &d);
        reg_get_int(&gReg, "FullScreen", &full);
        reg_get_int(&gReg, "UseDesktopRes", &desk);
    }
    buildResList(w, h);

    /* Menus must exist and be registered before the popup controls that use
       them, or the controls come up empty. */
    CreateNewMenu(MENU_RES, 0, &rm);
    for (i = 0; i < gResCount; i++) {
        CFStringRef t = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d x %d"),
                                                 gRes[i].w, gRes[i].h);
        AppendMenuItemTextWithCFString(rm, t, 0, 0, NULL);
        CFRelease(t);
        if (gRes[i].w == w && gRes[i].h == h) sel = i + 1;
    }
    InsertMenu(rm, kInsertHierarchicalMenu);

    CreateNewMenu(MENU_DEPTH, 0, &dm);
    AppendMenuItemTextWithCFString(dm, CFSTR("Thousands (16 bit)"), 0, 0, NULL);
    AppendMenuItemTextWithCFString(dm, CFSTR("Millions (32 bit)"),  0, 0, NULL);
    InsertMenu(dm, kInsertHierarchicalMenu);

    SetRect(&r, 0, 0, WIN_W, WIN_H);
    CreateNewWindow(kDocumentWindowClass,
                    kWindowStandardHandlerAttribute | kWindowCloseBoxAttribute,
                    &r, &win);
    SetWindowTitleWithCFString(win, CFSTR("Black & White Display"));
    CreateRootControl(win, &root);

    label(win, 20, 18, 430, 18, "Display settings for Black & White");

    label(win, 20, 58, 100, 18, "Resolution:");
    SetRect(&r, 130, 54, 320, 74);
    CreatePopupButtonControl(win, &r, NULL, MENU_RES, false, 0, 0, 0, &gResPop);
    SetControl32BitMaximum(gResPop, gResCount);
    SetControl32BitValue(gResPop, sel);

    label(win, 20, 92, 100, 18, "Colours:");
    SetRect(&r, 130, 88, 320, 108);
    CreatePopupButtonControl(win, &r, NULL, MENU_DEPTH, false, 0, 0, 0, &gDepthPop);
    SetControl32BitMaximum(gDepthPop, 2);
    SetControl32BitValue(gDepthPop, d >= 32 ? 2 : 1);

    SetRect(&r, 130, 122, 440, 140);
    CreateCheckBoxControl(win, &r, CFSTR("Full screen"), full ? 1 : 0, true, &gFullChk);

    SetRect(&r, 130, 146, 440, 164);
    CreateCheckBoxControl(win, &r, CFSTR("Just use the desktop resolution"),
                          desk ? 1 : 0, true, &gDeskChk);

    gStatus = label(win, 20, 180, 430, 46, "");

    SetRect(&r, 270, 246, 350, 266);
    CreatePushButtonControl(win, &r, CFSTR("Quit"), &btn);
    SetControlCommandID(btn, CMD_CANCEL);
    SetWindowCancelButton(win, btn);

    SetRect(&r, 362, 246, 442, 266);
    CreatePushButtonControl(win, &r, CFSTR("Save"), &btn);
    SetControlCommandID(btn, CMD_SAVE);
    SetWindowDefaultButton(win, btn);

    if (gHaveFile)
        snprintf(msg, sizeof(msg), "Currently %d x %d, %s, %s.",
                 w, h, d >= 32 ? "millions of colours" : "thousands of colours",
                 full ? "full screen" : "in a window");
    else
        snprintf(msg, sizeof(msg),
                 "No settings file yet. Run the game once, then come back.");
    setStatus(msg);

    InstallApplicationEventHandler(NewEventHandlerUPP(onCommand), 1, &spec, NULL, NULL);
    RepositionWindow(win, NULL, kWindowCenterOnMainScreen);
    ShowWindow(win);
    RunApplicationEventLoop();
    reg_free(&gReg);
    return 0;
}
