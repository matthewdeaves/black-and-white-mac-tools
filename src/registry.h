/*
 * The Mac port of Black & White stores its settings in an XML file that
 * imitates the Windows registry:
 *
 *   ~/Library/Preferences/Lionhead/Black & White/Preferences Data
 *
 * Values look like:
 *   <value name="ScreenW" type="integer">800</value>
 *
 * Only integer values are needed here, and only ones that already exist: the
 * game writes the file, this edits it. Keys are not created.
 */
#ifndef OMGP_REGISTRY_H
#define OMGP_REGISTRY_H

#define REG_OK        0
#define REG_NOFILE   -1
#define REG_NOKEY    -2
#define REG_IO       -3
#define REG_TOOBIG   -4

typedef struct {
    char  *text;      /* whole file, NUL terminated */
    long   len;
    char   path[1024];
} reg_file;

/* Default location under the calling user's home directory. */
void reg_default_path(char *out, unsigned long cap);

int  reg_load(const char *path, reg_file *r);
void reg_free(reg_file *r);

/* Read an integer value by key name. */
int  reg_get_int(const reg_file *r, const char *key, int *out);

/* Replace an existing integer value. Grows the buffer if needed. */
int  reg_set_int(reg_file *r, const char *key, int value);

/* Write back, after copying the original alongside as "<name>.orig" once. */
int  reg_save(reg_file *r);

#endif
