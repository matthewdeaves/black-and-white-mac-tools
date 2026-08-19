/*
 * Black & White Patcher
 *
 * Applies the fix that lets Black & White and Creature Isle run on Mac OS X
 * 10.5 Leopard. Finds the game itself, or you can choose a folder or drop one
 * on the app. Runs from anywhere.
 *
 * Carbon, so it runs on 10.3 through 10.6 on both PowerPC and Intel.
 */
#include <Carbon/Carbon.h>
#include <dirent.h>
#include <sys/stat.h>
#include "pefpatch.h"

#define CMD_PATCH   'ptch'
#define CMD_REVERT  'rvrt'
#define CMD_CHOOSE  'chos'

#define MAX_FOUND 16

static char       gFolder[1024];
static ControlRef gPathTxt, gListTxt, gPatchBtn, gRevertBtn;
static char       gFound[MAX_FOUND][1024];
static int        gFoundCount, gPatchedCount, gUnpatchedCount;

/* ---- helpers ------------------------------------------------------------- */

static void setText(ControlRef c, const char *s)
{
    CFStringRef cf = CFStringCreateWithCString(NULL, s, kCFStringEncodingUTF8);
    if (!cf) cf = CFStringCreateWithCString(NULL, s, kCFStringEncodingMacRoman);
    SetControlData(c, kControlEntireControl, kControlStaticTextCFStringTag,
                   sizeof(cf), &cf);
    Draw1Control(c);
    if (cf) CFRelease(cf);
}

static void walk(const char *path, int depth)
{
    struct stat st;
    if (gFoundCount >= MAX_FOUND || depth > 6) return;
    if (stat(path, &st) != 0) return;
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        struct dirent *e;
        if (!d) return;
        while ((e = readdir(d)) != NULL) {
            char child[1024];
            size_t nl = strlen(e->d_name);
            if (e->d_name[0] == '.') continue;
            if (nl > 8 && strcmp(e->d_name + nl - 8, ".omgpbak") == 0) continue;
            if (nl > 5 && strcmp(e->d_name + nl - 5, ".orig") == 0) continue;
            if (strlen(path) + nl + 2 >= sizeof(child)) continue;
            sprintf(child, "%s/%s", path, e->d_name);
            walk(child, depth + 1);
        }
        closedir(d);
        return;
    }
    if (!omgp_is_pef(path)) return;
    {
        unsigned char *b = NULL; unsigned long n = 0; omgp_info in;
        if (omgp_read_file(path, &b, &n) != OMGP_OK) return;
        if (omgp_scan(b, n, &in) == OMGP_OK) {
            strncpy(gFound[gFoundCount], path, sizeof(gFound[0]) - 1);
            gFound[gFoundCount][sizeof(gFound[0]) - 1] = 0;
            gFoundCount++;
            if (in.patched) gPatchedCount++; else gUnpatchedCount++;
        }
        free(b);
    }
}

static void rescan(void)
{
    char summary[4096];
    int i;

    gFoundCount = gPatchedCount = gUnpatchedCount = 0;
    summary[0] = 0;

    if (gFolder[0]) walk(gFolder, 0);

    setText(gPathTxt, gFolder[0] ? gFolder : "No game folder chosen yet.");

    if (gFoundCount == 0) {
        strcpy(summary,
            "No Black & White or Creature Isle program found here.\r"
            "\r"
            "Choose the folder the game is installed in, or drag it onto this app.");
    } else {
        for (i = 0; i < gFoundCount; i++) {
            const char *name = strrchr(gFound[i], '/');
            unsigned char *b = NULL; unsigned long n = 0; omgp_info in;
            char line[512];
            name = name ? name + 1 : gFound[i];
            if (omgp_read_file(gFound[i], &b, &n) != OMGP_OK) continue;
            omgp_scan(b, n, &in);
            free(b);
            snprintf(line, sizeof(line), "%s  -  %s\r",
                     name, in.patched ? "already fixed" : "needs the fix");
            if (strlen(summary) + strlen(line) < sizeof(summary)) strcat(summary, line);
        }
    }
    setText(gListTxt, summary);

    if (gUnpatchedCount > 0) EnableControl(gPatchBtn); else DisableControl(gPatchBtn);
    if (gPatchedCount  > 0) EnableControl(gRevertBtn); else DisableControl(gRevertBtn);
}

static void applyAll(int revert)
{
    char summary[4096];
    int i, done = 0, failed = 0;
    char line[512];

    summary[0] = 0;
    for (i = 0; i < gFoundCount; i++) {
        omgp_info in;
        int rc = revert ? omgp_revert(gFound[i], &in) : omgp_patch(gFound[i], &in);
        const char *name = strrchr(gFound[i], '/');
        name = name ? name + 1 : gFound[i];
        if (rc == OMGP_OK) {
            done++;
            snprintf(line, sizeof(line), "%s  -  %s\r", name, revert ? "put back" : "fixed");
        } else if (rc == OMGP_STATE) {
            snprintf(line, sizeof(line), "%s  -  nothing to do\r", name);
        } else {
            failed++;
            snprintf(line, sizeof(line), "%s  -  %s\r", name, omgp_strerror(rc));
        }
        if (strlen(summary) + strlen(line) < sizeof(summary)) strcat(summary, line);
    }
    if (done && !failed) {
        strcat(summary, "\rQuit the game first if it is running, then launch it.");
    }
    rescan();
    setText(gListTxt, summary);
}

static void chooseFolder(void)
{
    NavDialogCreationOptions opts;
    NavDialogRef dlg = NULL;
    NavReplyRecord reply;
    AEKeyword kw; DescType t; FSRef ref; Size sz;

    if (NavGetDefaultDialogCreationOptions(&opts) != noErr) return;
    opts.modality = kWindowModalityAppModal;
    opts.message = CFSTR("Choose the folder Black & White is installed in.");
    if (NavCreateChooseFolderDialog(&opts, NULL, NULL, NULL, &dlg) != noErr) return;
    if (NavDialogRun(dlg) == noErr) {
        if (NavDialogGetReply(dlg, &reply) == noErr) {
            if (reply.validRecord &&
                AEGetNthPtr(&reply.selection, 1, typeFSRef, &kw, &t,
                            &ref, sizeof(ref), &sz) == noErr) {
                if (FSRefMakePath(&ref, (UInt8 *)gFolder, sizeof(gFolder)) == noErr)
                    rescan();
            }
            NavDisposeReply(&reply);
        }
    }
    NavDialogDispose(dlg);
}

/* ---- dropped folders ----------------------------------------------------- */

static pascal OSErr onOpenDocs(const AppleEvent *ae, AppleEvent *reply, long ref)
{
    AEDescList docs;
    long n; FSRef fs;
    AEKeyword kw; DescType t; Size sz;
    (void)reply; (void)ref;

    if (AEGetParamDesc(ae, keyDirectObject, typeAEList, &docs) != noErr) return noErr;
    if (AECountItems(&docs, &n) == noErr && n > 0) {
        if (AEGetNthPtr(&docs, 1, typeFSRef, &kw, &t, &fs, sizeof(fs), &sz) == noErr) {
            if (FSRefMakePath(&fs, (UInt8 *)gFolder, sizeof(gFolder)) == noErr)
                rescan();
        }
    }
    AEDisposeDesc(&docs);
    return noErr;
}

/* ---- ui ------------------------------------------------------------------ */

static OSStatus onCommand(EventHandlerCallRef h, EventRef e, void *ud)
{
    HICommand c;
    (void)h; (void)ud;
    if (GetEventParameter(e, kEventParamDirectObject, typeHICommand, NULL,
                          sizeof(c), NULL, &c) != noErr) return eventNotHandledErr;
    switch (c.commandID) {
    case CMD_PATCH:  applyAll(0);               return noErr;
    case CMD_REVERT: applyAll(1);               return noErr;
    case CMD_CHOOSE: chooseFolder();            return noErr;
    case kHICommandQuit: QuitApplicationEventLoop(); return noErr;
    }
    return eventNotHandledErr;
}

static ControlRef label(WindowRef w, int l, int t, int width, int height, const char *s)
{
    Rect r; ControlRef c; CFStringRef cf;
    SetRect(&r, l, t, l + width, t + height);
    cf = CFStringCreateWithCString(NULL, s, kCFStringEncodingUTF8);
    CreateStaticTextControl(w, &r, cf, NULL, &c);
    CFRelease(cf);
    return c;
}

static void autoDetect(void)
{
    const char *home = getenv("HOME");
    char cand[8][1024];
    int n = 0, i;
    struct stat st;

    snprintf(cand[n++], 1024, "/Applications/Black & White");
    snprintf(cand[n++], 1024, "/Applications/Games/Black & White");
    if (home) {
        snprintf(cand[n++], 1024, "%s/Desktop/Black & White", home);
        snprintf(cand[n++], 1024, "%s/Applications/Black & White", home);
    }
    for (i = 0; i < n; i++) {
        if (stat(cand[i], &st) == 0 && S_ISDIR(st.st_mode)) {
            strncpy(gFolder, cand[i], sizeof(gFolder) - 1);
            return;
        }
    }
}

int main(void)
{
    WindowRef win;
    ControlRef root, btn;
    Rect r;
    EventTypeSpec spec = { kEventClassCommand, kEventProcessCommand };

    SetRect(&r, 0, 0, 530, 300);
    CreateNewWindow(kDocumentWindowClass,
                    kWindowStandardHandlerAttribute | kWindowCloseBoxAttribute,
                    &r, &win);
    SetWindowTitleWithCFString(win, CFSTR("Black & White Patcher"));
    CreateRootControl(win, &root);

    label(win, 20, 16, 490, 32,
          "Lets Black & White and Creature Isle run on Mac OS X 10.5 Leopard.");

    label(win, 20, 56, 100, 16, "Game folder:");
    gPathTxt = label(win, 20, 74, 490, 16, "");

    gListTxt = label(win, 20, 102, 490, 100, "");

    SetRect(&r, 20, 210, 170, 230);
    CreatePushButtonControl(win, &r, CFSTR("Choose Folder..."), &btn);
    SetControlCommandID(btn, CMD_CHOOSE);

    SetRect(&r, 300, 210, 400, 230);
    CreatePushButtonControl(win, &r, CFSTR("Put Back"), &gRevertBtn);
    SetControlCommandID(gRevertBtn, CMD_REVERT);

    SetRect(&r, 410, 210, 510, 230);
    CreatePushButtonControl(win, &r, CFSTR("Fix"), &gPatchBtn);
    SetControlCommandID(gPatchBtn, CMD_PATCH);
    SetWindowDefaultButton(win, gPatchBtn);

    label(win, 20, 246, 490, 32,
          "A copy of the changed bytes is kept, so Put Back always works.");

    AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments,
                          NewAEEventHandlerUPP(onOpenDocs), 0, false);

    autoDetect();
    rescan();

    InstallApplicationEventHandler(NewEventHandlerUPP(onCommand), 1, &spec, NULL, NULL);
    RepositionWindow(win, NULL, kWindowCenterOnMainScreen);
    ShowWindow(win);
    RunApplicationEventLoop();
    return 0;
}
