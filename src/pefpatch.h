/*
 * pefpatch - locate and apply the Black & White / Creature Isle null-pointer
 * fix inside a PowerPC PEF (CFM) executable.
 *
 * The patch site is found structurally, from the binary's own PowerPC
 * traceback tables, so it works on builds this tool has never seen.
 * Nothing is written unless every structural check passes.
 */
#ifndef OMGP_PEFPATCH_H
#define OMGP_PEFPATCH_H

#define OMGP_OK              0
#define OMGP_NOT_PEF        -1   /* not a PowerPC PEF container        */
#define OMGP_NO_CODE        -2   /* no uncompressed code section       */
#define OMGP_NO_SYMBOL      -3   /* target symbol absent or ambiguous  */
#define OMGP_NO_MARKER      -4   /* traceback table not where expected */
#define OMGP_NO_ENTRY       -5   /* function prologue did not match    */
#define OMGP_CAVE_SMALL     -6   /* not enough dead space for the stub */
#define OMGP_IO             -7   /* read/write failure                 */
#define OMGP_STATE          -8   /* file not in the expected state     */
#define OMGP_NOBACKUP    -9   /* no undo record beside the file     */

#define OMGP_STUB_LEN  44
#define OMGP_DELTA     0x38      /* stub sits this far below the entry */

typedef struct {
    int           patched;       /* 1 = the fix is already present     */
    unsigned long code_begin;    /* code section, file offsets         */
    unsigned long code_end;
    unsigned long entry;         /* first instruction of GetPrimaryKey */
    unsigned long cave;          /* start of the dead traceback table  */
    unsigned long cave_size;
    unsigned long stub_at;       /* where the stub is written          */
    char          md5[33];
    char          prev_symbol[128]; /* function that owns the cave     */
} omgp_info;

const char *omgp_strerror(int err);

/* Read a whole file. Caller frees with free(). */
int  omgp_read_file(const char *path, unsigned char **buf, unsigned long *len);

/* Inspect a buffer. Fills info. Returns OMGP_OK or a negative code. */
int  omgp_scan(const unsigned char *d, unsigned long n, omgp_info *info);

/* Apply / undo. Both verify state before and after writing. */
int  omgp_patch(const char *path, omgp_info *info);

/*
 * Undo. Prefers the .omgpbak record written by omgp_patch, which restores the
 * file byte for byte. Without one, it can still put the game's own instruction
 * back and clear the stub: the region the stub occupies is a traceback table,
 * which is debug metadata nothing reads at runtime, so the game behaves as it
 * did before even though the file will not match its original md5.
 *
 * Sets *exact to 1 when an undo record was used, 0 when it was reconstructed.
 */
int  omgp_revert(const char *path, omgp_info *info, int *exact);

/* Is this file a PowerPC PEF at all? Cheap check for directory walks. */
int  omgp_is_pef(const char *path);

#endif
