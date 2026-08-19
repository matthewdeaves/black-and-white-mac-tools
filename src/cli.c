/*
 * oldmacpatch - command line front end.
 *
 *   oldmacpatch scan   <file-or-folder>
 *   oldmacpatch patch  <file-or-folder>
 *   oldmacpatch revert <file-or-folder>
 */
#include "pefpatch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define VERSION "1.0"

static int verbose = 0;

static void report(const char *path, omgp_info *in, int rc, const char *action)
{
    printf("%s\n", path);
    if (rc != OMGP_OK) {
        printf("    skipped: %s\n", omgp_strerror(rc));
        return;
    }
    printf("    md5        %s\n", in->md5);
    printf("    state      %s\n", in->patched ? "patched" : "unpatched");
    if (verbose) {
        printf("    code       0x%lx-0x%lx\n", in->code_begin, in->code_end);
        printf("    entry      0x%lx\n", in->entry);
        printf("    cave       0x%lx, %lu bytes, in %s\n",
               in->cave, in->cave_size,
               in->prev_symbol[0] ? in->prev_symbol : "(unnamed)");
        printf("    stub at    0x%lx\n", in->stub_at);
    }
    if (action) printf("    %s\n", action);
}

static int do_one(const char *path, const char *cmd)
{
    omgp_info in;
    unsigned char *d = NULL;
    unsigned long n = 0;
    int rc;

    if (strcmp(cmd, "scan") == 0) {
        rc = omgp_read_file(path, &d, &n);
        if (rc == OMGP_OK) { rc = omgp_scan(d, n, &in); free(d); }
        report(path, &in, rc, NULL);
        return rc == OMGP_OK ? 0 : 1;
    }
    if (strcmp(cmd, "patch") == 0) {
        rc = omgp_patch(path, &in);
        if (rc == OMGP_STATE && in.patched)
            { report(path, &in, OMGP_OK, "already patched, nothing to do"); return 0; }
        report(path, &in, rc, rc == OMGP_OK ? "patched, verified on disk" : NULL);
        return rc == OMGP_OK ? 0 : 1;
    }
    if (strcmp(cmd, "revert") == 0) {
        rc = omgp_revert(path, &in);
        if (rc == OMGP_STATE && !in.patched)
            { report(path, &in, OMGP_OK, "not patched, nothing to do"); return 0; }
        report(path, &in, rc, rc == OMGP_OK ? "reverted, verified on disk" : NULL);
        return rc == OMGP_OK ? 0 : 1;
    }
    fprintf(stderr, "unknown command: %s\n", cmd);
    return 2;
}

static int walk(const char *path, const char *cmd, int *seen)
{
    struct stat st;
    int bad = 0;

    if (stat(path, &st) != 0) {
        fprintf(stderr, "cannot read %s\n", path);
        return 1;
    }
    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        struct dirent *ent;
        if (!dir) return 1;
        while ((ent = readdir(dir)) != NULL) {
            char child[2048];
            size_t nl = strlen(ent->d_name);
            if (ent->d_name[0] == '.') continue;
            /* our own undo records, which we may delete mid-walk */
            if (nl > 8 && strcmp(ent->d_name + nl - 8, ".omgpbak") == 0) continue;
            if (strlen(path) + strlen(ent->d_name) + 2 >= sizeof(child)) continue;
            sprintf(child, "%s/%s", path, ent->d_name);
            bad |= walk(child, cmd, seen);
        }
        closedir(dir);
        return bad;
    }
    if (!omgp_is_pef(path)) return 0;
    (*seen)++;
    return do_one(path, cmd);
}

int main(int argc, char **argv)
{
    int i, seen = 0, bad;
    const char *cmd = NULL, *target = NULL;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) verbose = 1;
        else if (strcmp(argv[i], "--version") == 0) { printf("oldmacpatch %s\n", VERSION); return 0; }
        else if (!cmd) cmd = argv[i];
        else if (!target) target = argv[i];
    }
    if (!cmd || !target) {
        fprintf(stderr,
            "oldmacpatch %s\n\n"
            "  oldmacpatch scan   <file-or-folder>   report what is found\n"
            "  oldmacpatch patch  <file-or-folder>   apply the fix\n"
            "  oldmacpatch revert <file-or-folder>   undo it\n\n"
            "  -v   show offsets\n", VERSION);
        return 2;
    }
    bad = walk(target, cmd, &seen);
    if (seen == 0) { printf("no PowerPC PEF executables found under %s\n", target); return 1; }
    printf("\n%d executable%s examined\n", seen, seen == 1 ? "" : "s");
    return bad;
}
