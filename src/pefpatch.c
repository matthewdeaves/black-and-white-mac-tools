#include "pefpatch.h"
#include "md5.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- what we are looking for -------------------------------------------- */

/* BindableAction::GetPrimaryKey() const, as it appears in a traceback table */
static const char SYMBOL[] = ".GetPrimaryKey__14BindableActionCFv";

/* Its first four instructions. Any build whose prologue differs is refused. */
static const unsigned long PROLOGUE[4] = {
    0x80a40100UL,   /* lwz   r5,0x100(r4)  <- the faulting load */
    0x38000010UL,   /* li    r0,16                              */
    0x7c671b78UL,   /* mr    r7,r3                              */
    0x38c40100UL    /* addi  r6,r4,0x100                        */
};

#define BLR      0x4e800020UL
#define DETOUR   0x4bffffc8UL   /* b -0x38, overwrites the faulting load */

/*
 * The guard. Rejects a "this" below 0x10000, which is what a null base plus a
 * member offset produces, and returns an all-zero 264 byte key instead of
 * reading off the end of nothing. A real pointer takes the original path.
 *
 *   cmplwi r4,0xffff        bgt    +0x20
 *   li     r0,66            mtctr  r0
 *   addi   r5,r3,-4         li     r0,0
 *   stwu   r0,4(r5)         bdnz   -4
 *   blr
 *   lwz    r5,0x100(r4)     b      +0x14
 *
 * Every branch is PC relative, so the same bytes work wherever the cave is,
 * as long as the stub sits exactly OMGP_DELTA below the entry.
 */
static const unsigned char STUB[OMGP_STUB_LEN] = {
    0x28,0x04,0xff,0xff, 0x41,0x81,0x00,0x20, 0x38,0x00,0x00,0x42, 0x7c,0x09,0x03,0xa6,
    0x38,0xa3,0xff,0xfc, 0x38,0x00,0x00,0x00, 0x94,0x05,0x00,0x04, 0x42,0x00,0xff,0xfc,
    0x4e,0x80,0x00,0x20, 0x80,0xa4,0x01,0x00, 0x48,0x00,0x00,0x14
};

/* ---- helpers ------------------------------------------------------------- */

static unsigned long be32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) |
           ((unsigned long)p[2] << 8)  |  (unsigned long)p[3];
}

static unsigned int be16(const unsigned char *p)
{
    return ((unsigned int)p[0] << 8) | (unsigned int)p[1];
}

const char *omgp_strerror(int err)
{
    switch (err) {
    case OMGP_OK:          return "ok";
    case OMGP_NOT_PEF:     return "not a PowerPC PEF executable";
    case OMGP_NO_CODE:     return "no uncompressed code section";
    case OMGP_NO_SYMBOL:   return "GetPrimaryKey symbol absent or ambiguous";
    case OMGP_NO_MARKER:   return "traceback table not where expected";
    case OMGP_NO_ENTRY:    return "function prologue did not match";
    case OMGP_CAVE_SMALL:  return "not enough dead space for the stub";
    case OMGP_IO:          return "read or write failure";
    case OMGP_STATE:       return "file is not in the expected state";
    case OMGP_NOBACKUP:    return "no undo record beside this file";
    }
    return "unknown error";
}

int omgp_read_file(const char *path, unsigned char **buf, unsigned long *len)
{
    FILE *f; long n; unsigned char *b;
    f = fopen(path, "rb");
    if (!f) return OMGP_IO;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return OMGP_IO; }
    n = ftell(f);
    if (n <= 0) { fclose(f); return OMGP_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return OMGP_IO; }
    b = (unsigned char *)malloc((size_t)n);
    if (!b) { fclose(f); return OMGP_IO; }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return OMGP_IO; }
    fclose(f);
    *buf = b; *len = (unsigned long)n;
    return OMGP_OK;
}

int omgp_is_pef(const char *path)
{
    FILE *f; char h[12]; size_t got;
    f = fopen(path, "rb");
    if (!f) return 0;
    got = fread(h, 1, 12, f);
    fclose(f);
    return got == 12 && memcmp(h, "Joy!peffpwpc", 12) == 0;
}

/* ---- discovery ----------------------------------------------------------- */

static int find_code_section(const unsigned char *d, unsigned long n,
                             unsigned long *begin, unsigned long *end)
{
    unsigned int count, i;
    if (n < 0x28 || memcmp(d, "Joy!peffpwpc", 12) != 0) return OMGP_NOT_PEF;
    count = be16(d + 0x20);
    for (i = 0; i < count; i++) {
        unsigned long o = 0x28UL + (unsigned long)i * 28UL;
        unsigned long total, unpacked, packed, coff;
        if (o + 28 > n) return OMGP_NOT_PEF;
        total    = be32(d + o + 8);
        unpacked = be32(d + o + 12);
        packed   = be32(d + o + 16);
        coff     = be32(d + o + 20);
        if (d[o + 24] != 0) continue;              /* kind 0 = code */
        if (packed != unpacked || packed != total) return OMGP_NO_CODE;
        if (coff + packed > n) return OMGP_NO_CODE;
        *begin = coff; *end = coff + packed;
        return OMGP_OK;
    }
    return OMGP_NO_CODE;
}

/* Locate the single traceback entry naming SYMBOL. Returns its length field. */
static int find_symbol(const unsigned char *d, unsigned long b, unsigned long e,
                       unsigned long *at)
{
    unsigned long i, hit = 0; int found = 0;
    unsigned int len = (unsigned int)strlen(SYMBOL);
    for (i = b; i + 2 + len <= e; i++) {
        if (be16(d + i) != len) continue;
        if (memcmp(d + i + 2, SYMBOL, len) != 0) continue;
        found++; hit = i;
        if (found > 1) return OMGP_NO_SYMBOL;
    }
    if (found != 1) return OMGP_NO_SYMBOL;
    *at = hit;
    return OMGP_OK;
}

/*
 * A traceback table starts with a zero word, and the instruction directly
 * before it is the function's last, a blr. Walk back from the name to find it.
 */
static int find_marker(const unsigned char *d, unsigned long b,
                       unsigned long name_at, unsigned long *marker)
{
    unsigned long p = name_at & ~3UL;
    unsigned long limit = (p > b + 0x100UL) ? (p - 0x100UL) : b;
    for (; p > limit + 4; p -= 4) {
        if (be32(d + p) == 0 && be32(d + p - 4) == BLR) { *marker = p; return OMGP_OK; }
    }
    return OMGP_NO_MARKER;
}

static int match_words(const unsigned char *d, unsigned long at,
                       const unsigned long *words, int n)
{
    int i;
    for (i = 0; i < n; i++)
        if (be32(d + at + (unsigned long)i * 4) != words[i]) return 0;
    return 1;
}

int omgp_scan(const unsigned char *d, unsigned long n, omgp_info *info)
{
    unsigned long b, e, name_at, marker, p, limit, entry = 0, cave = 0;
    int rc, patched = 0;

    memset(info, 0, sizeof(*info));
    md5_hex(d, n, info->md5);

    rc = find_code_section(d, n, &b, &e);
    if (rc != OMGP_OK) return rc;
    info->code_begin = b; info->code_end = e;

    rc = find_symbol(d, b, e, &name_at);
    if (rc != OMGP_OK) return rc;

    rc = find_marker(d, b, name_at, &marker);
    if (rc != OMGP_OK) return rc;

    /* Walk back from the last instruction looking for the prologue, or, if the
       fix is already in, for the detour branch we would have written. */
    limit = (marker > b + 0x400UL) ? (marker - 0x400UL) : b;
    for (p = marker - 8; p > limit; p -= 4) {
        if (match_words(d, p, PROLOGUE, 4)) { entry = p; break; }
        if (be32(d + p) == DETOUR && p >= OMGP_DELTA &&
            be32(d + p - OMGP_DELTA) == 0x2804ffffUL) {
            entry = p; patched = 1; break;
        }
    }
    if (!entry) return OMGP_NO_ENTRY;
    info->entry = entry;
    info->patched = patched;

    info->stub_at = entry - OMGP_DELTA;

    /*
     * On a patched file the original traceback table is underneath our stub and
     * cannot be recovered, so report the region we occupy and say nothing about
     * whose it was. Scanning here would walk straight past the stub and name
     * some earlier function, which is how this first reported itself.
     */
    if (patched) {
        info->cave = info->stub_at;
        info->cave_size = OMGP_DELTA;
        return OMGP_OK;
    }

    /* The cave is the previous function's traceback table: dead metadata that
       begins with a zero word and is preceded by that function's blr. */
    limit = (entry > b + 0x400UL) ? (entry - 0x400UL) : b;
    for (p = entry - 4; p > limit + 4; p -= 4) {
        if (be32(d + p) == 0 && be32(d + p - 4) == BLR) { cave = p; break; }
    }
    if (!cave) return OMGP_NO_MARKER;
    info->cave = cave;
    info->cave_size = entry - cave;
    if (info->cave_size < OMGP_DELTA) return OMGP_CAVE_SMALL;

    /* Name the function whose traceback table we are borrowing, for the log. */
    {
        unsigned long q, k; unsigned int l; int ok;
        for (q = cave; q + 2 < entry; q++) {
            l = be16(d + q);
            if (l < 4 || l >= sizeof(info->prev_symbol)) continue;
            if (q + 2 + l > entry || d[q + 2] != '.') continue;
            ok = 1;
            for (k = 0; k < l; k++) {
                unsigned char c = d[q + 2 + k];
                if (c < 0x21 || c > 0x7e) { ok = 0; break; }
            }
            if (!ok) continue;
            memcpy(info->prev_symbol, d + q + 2, l);
            info->prev_symbol[l] = 0;
            break;
        }
    }
    return OMGP_OK;
}

/* ---- writing ------------------------------------------------------------- */

static int write_at(const char *path, unsigned long off,
                    const unsigned char *bytes, unsigned long len)
{
    FILE *f = fopen(path, "r+b");
    if (!f) return OMGP_IO;
    if (fseek(f, (long)off, SEEK_SET) != 0) { fclose(f); return OMGP_IO; }
    if (fwrite(bytes, 1, (size_t)len, f) != (size_t)len) { fclose(f); return OMGP_IO; }
    if (fclose(f) != 0) return OMGP_IO;
    return OMGP_OK;
}

static void undo_path(const char *path, char *out, unsigned long cap)
{
    unsigned long n = (unsigned long)strlen(path);
    if (n + 8 >= cap) n = cap - 9;
    memcpy(out, path, (size_t)n);
    memcpy(out + n, ".omgpbak", 9);
}

int omgp_patch(const char *path, omgp_info *info)
{
    unsigned char *d = NULL, detour[4];
    unsigned long n;
    char ubuf[2048];
    int rc;
    FILE *u;

    rc = omgp_read_file(path, &d, &n);
    if (rc != OMGP_OK) return rc;
    rc = omgp_scan(d, n, info);
    if (rc != OMGP_OK) { free(d); return rc; }
    if (info->patched) { free(d); return OMGP_STATE; }

    /* Save the bytes we are about to overwrite, so revert needs no full copy
       and therefore cannot lose the file's resource fork. */
    undo_path(path, ubuf, sizeof(ubuf));
    u = fopen(ubuf, "wb");
    if (!u) { free(d); return OMGP_IO; }
    fprintf(u, "oldmacpatch-undo 1\nmd5 %s\nstub %lu\nentry %lu\n",
            info->md5, info->stub_at, info->entry);
    fwrite(d + info->stub_at, 1, OMGP_STUB_LEN, u);
    fwrite(d + info->entry, 1, 4, u);
    if (fclose(u) != 0) { free(d); return OMGP_IO; }
    free(d);

    rc = write_at(path, info->stub_at, STUB, OMGP_STUB_LEN);
    if (rc != OMGP_OK) return rc;
    detour[0] = 0x4b; detour[1] = 0xff; detour[2] = 0xff; detour[3] = 0xc8;
    rc = write_at(path, info->entry, detour, 4);
    if (rc != OMGP_OK) return rc;

    /* Confirm from disk. */
    rc = omgp_read_file(path, &d, &n);
    if (rc != OMGP_OK) return rc;
    rc = omgp_scan(d, n, info);
    free(d);
    if (rc != OMGP_OK) return rc;
    return info->patched ? OMGP_OK : OMGP_STATE;
}

int omgp_revert(const char *path, omgp_info *info, int *exact)
{
    unsigned char *d = NULL, saved[OMGP_STUB_LEN + 4], blank[OMGP_STUB_LEN];
    unsigned long n, stub_at = 0, entry = 0;
    char ubuf[2048], line[256], md5[64];
    int rc, have_record = 0;
    FILE *u;

    if (exact) *exact = 0;

    rc = omgp_read_file(path, &d, &n);
    if (rc != OMGP_OK) return rc;
    rc = omgp_scan(d, n, info);
    free(d);
    if (rc != OMGP_OK) return rc;
    if (!info->patched) return OMGP_STATE;

    md5[0] = 0;
    undo_path(path, ubuf, sizeof(ubuf));
    u = fopen(ubuf, "rb");
    if (u) {
        if (fgets(line, sizeof(line), u) &&
            strncmp(line, "oldmacpatch-undo 1", 18) == 0 &&
            fscanf(u, "md5 %63s\nstub %lu\nentry %lu\n", md5, &stub_at, &entry) == 3 &&
            stub_at == info->stub_at && entry == info->entry &&
            fread(saved, 1, OMGP_STUB_LEN + 4, u) == OMGP_STUB_LEN + 4) {
            have_record = 1;
        }
        fclose(u);
    }

    if (!have_record) {
        /* Reconstruct. The stub sits in a traceback table, so clearing it costs
           only debug metadata; putting the entry instruction back is what
           actually matters. */
        memset(blank, 0, sizeof(blank));
        memcpy(saved, blank, OMGP_STUB_LEN);
        saved[OMGP_STUB_LEN + 0] = 0x80; saved[OMGP_STUB_LEN + 1] = 0xa4;
        saved[OMGP_STUB_LEN + 2] = 0x01; saved[OMGP_STUB_LEN + 3] = 0x00;
        stub_at = info->stub_at;
        entry   = info->entry;
    }

    rc = write_at(path, stub_at, saved, OMGP_STUB_LEN);
    if (rc != OMGP_OK) return rc;
    rc = write_at(path, entry, saved + OMGP_STUB_LEN, 4);
    if (rc != OMGP_OK) return rc;

    rc = omgp_read_file(path, &d, &n);
    if (rc != OMGP_OK) return rc;
    rc = omgp_scan(d, n, info);
    free(d);
    if (rc != OMGP_OK) return rc;
    if (info->patched) return OMGP_STATE;

    if (have_record) {
        if (strcmp(info->md5, md5) != 0) return OMGP_STATE;
        remove(ubuf);
        if (exact) *exact = 1;
    }
    return OMGP_OK;
}
