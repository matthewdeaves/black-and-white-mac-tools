#include "registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void reg_default_path(char *out, unsigned long cap)
{
    const char *home = getenv("HOME");
    if (!home) home = "";
    snprintf(out, (size_t)cap,
             "%s/Library/Preferences/Lionhead/Black & White/Preferences Data", home);
}

int reg_load(const char *path, reg_file *r)
{
    FILE *f; long n;
    memset(r, 0, sizeof(*r));
    strncpy(r->path, path, sizeof(r->path) - 1);
    f = fopen(path, "rb");
    if (!f) return REG_NOFILE;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return REG_IO; }
    n = ftell(f);
    if (n < 0 || n > 4L * 1024 * 1024) { fclose(f); return REG_TOOBIG; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return REG_IO; }
    r->text = (char *)malloc((size_t)n + 1);
    if (!r->text) { fclose(f); return REG_IO; }
    if (fread(r->text, 1, (size_t)n, f) != (size_t)n) {
        free(r->text); r->text = NULL; fclose(f); return REG_IO;
    }
    fclose(f);
    r->text[n] = 0;
    r->len = n;
    return REG_OK;
}

void reg_free(reg_file *r)
{
    if (r->text) free(r->text);
    r->text = NULL; r->len = 0;
}

/* Find the value text for a key. Returns pointer to the first digit, and sets
   *end just past the last, or NULL. */
static char *find_value(const reg_file *r, const char *key, char **end)
{
    char needle[256];
    char *p, *gt;
    if (!r->text) return NULL;
    snprintf(needle, sizeof(needle), "<value name=\"%s\" type=\"integer\">", key);
    p = strstr(r->text, needle);
    if (!p) return NULL;
    p += strlen(needle);
    gt = strstr(p, "</value>");
    if (!gt) return NULL;
    *end = gt;
    return p;
}

int reg_get_int(const reg_file *r, const char *key, int *out)
{
    char *end, *p = find_value(r, key, &end);
    if (!p) return REG_NOKEY;
    *out = atoi(p);
    return REG_OK;
}

int reg_set_int(reg_file *r, const char *key, int value)
{
    char buf[32];
    char *end, *p = find_value(r, key, &end);
    long before, after, oldlen, newlen;
    char *nt;

    if (!p) return REG_NOKEY;
    snprintf(buf, sizeof(buf), "%d", value);
    newlen = (long)strlen(buf);
    oldlen = (long)(end - p);
    if (newlen == oldlen) { memcpy(p, buf, (size_t)newlen); return REG_OK; }

    before = (long)(p - r->text);
    after  = r->len - (before + oldlen);
    nt = (char *)malloc((size_t)(r->len - oldlen + newlen + 1));
    if (!nt) return REG_IO;
    memcpy(nt, r->text, (size_t)before);
    memcpy(nt + before, buf, (size_t)newlen);
    memcpy(nt + before + newlen, end, (size_t)after);
    nt[r->len - oldlen + newlen] = 0;
    free(r->text);
    r->text = nt;
    r->len  = r->len - oldlen + newlen;
    return REG_OK;
}

int reg_save(reg_file *r)
{
    char bak[1100];
    FILE *f, *g;

    if (!r->text) return REG_IO;

    /* Keep one pristine copy, made the first time only. */
    snprintf(bak, sizeof(bak), "%s.orig", r->path);
    g = fopen(bak, "rb");
    if (g) {
        fclose(g);
    } else {
        FILE *src = fopen(r->path, "rb");
        if (src) {
            g = fopen(bak, "wb");
            if (g) {
                char b[8192]; size_t got;
                while ((got = fread(b, 1, sizeof(b), src)) > 0) fwrite(b, 1, got, g);
                fclose(g);
            }
            fclose(src);
        }
    }

    f = fopen(r->path, "wb");
    if (!f) return REG_IO;
    if (fwrite(r->text, 1, (size_t)r->len, f) != (size_t)r->len) { fclose(f); return REG_IO; }
    if (fclose(f) != 0) return REG_IO;
    return REG_OK;
}
