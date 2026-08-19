/* Minimal MD5, public domain style. Used only to identify binaries. */
#ifndef OMGP_MD5_H
#define OMGP_MD5_H

typedef struct {
    unsigned int  state[4];
    unsigned int  count[2];
    unsigned char buffer[64];
} md5_ctx;

void md5_init(md5_ctx *c);
void md5_update(md5_ctx *c, const unsigned char *data, unsigned long len);
void md5_final(md5_ctx *c, unsigned char digest[16]);
void md5_hex(const unsigned char *data, unsigned long len, char out[33]);

#endif
