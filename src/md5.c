#include "md5.h"
#include <string.h>

#define F(x,y,z) (((x) & (y)) | (~(x) & (z)))
#define G(x,y,z) (((x) & (z)) | ((y) & ~(z)))
#define H(x,y,z) ((x) ^ (y) ^ (z))
#define I(x,y,z) ((y) ^ ((x) | ~(z)))
#define ROT(x,n) (((x) << (n)) | ((x) >> (32-(n))))
#define STEP(f,a,b,c,d,x,t,s) \
    (a) += f((b),(c),(d)) + (x) + (t); (a) = ROT((a),(s)); (a) += (b);

static void md5_transform(unsigned int state[4], const unsigned char block[64])
{
    unsigned int a, b, c, d, x[16];
    int i;
    for (i = 0; i < 16; i++)
        x[i] =  (unsigned int)block[i*4]
             | ((unsigned int)block[i*4+1] << 8)
             | ((unsigned int)block[i*4+2] << 16)
             | ((unsigned int)block[i*4+3] << 24);
    a = state[0]; b = state[1]; c = state[2]; d = state[3];

    STEP(F,a,b,c,d,x[ 0],0xd76aa478, 7) STEP(F,d,a,b,c,x[ 1],0xe8c7b756,12)
    STEP(F,c,d,a,b,x[ 2],0x242070db,17) STEP(F,b,c,d,a,x[ 3],0xc1bdceee,22)
    STEP(F,a,b,c,d,x[ 4],0xf57c0faf, 7) STEP(F,d,a,b,c,x[ 5],0x4787c62a,12)
    STEP(F,c,d,a,b,x[ 6],0xa8304613,17) STEP(F,b,c,d,a,x[ 7],0xfd469501,22)
    STEP(F,a,b,c,d,x[ 8],0x698098d8, 7) STEP(F,d,a,b,c,x[ 9],0x8b44f7af,12)
    STEP(F,c,d,a,b,x[10],0xffff5bb1,17) STEP(F,b,c,d,a,x[11],0x895cd7be,22)
    STEP(F,a,b,c,d,x[12],0x6b901122, 7) STEP(F,d,a,b,c,x[13],0xfd987193,12)
    STEP(F,c,d,a,b,x[14],0xa679438e,17) STEP(F,b,c,d,a,x[15],0x49b40821,22)

    STEP(G,a,b,c,d,x[ 1],0xf61e2562, 5) STEP(G,d,a,b,c,x[ 6],0xc040b340, 9)
    STEP(G,c,d,a,b,x[11],0x265e5a51,14) STEP(G,b,c,d,a,x[ 0],0xe9b6c7aa,20)
    STEP(G,a,b,c,d,x[ 5],0xd62f105d, 5) STEP(G,d,a,b,c,x[10],0x02441453, 9)
    STEP(G,c,d,a,b,x[15],0xd8a1e681,14) STEP(G,b,c,d,a,x[ 4],0xe7d3fbc8,20)
    STEP(G,a,b,c,d,x[ 9],0x21e1cde6, 5) STEP(G,d,a,b,c,x[14],0xc33707d6, 9)
    STEP(G,c,d,a,b,x[ 3],0xf4d50d87,14) STEP(G,b,c,d,a,x[ 8],0x455a14ed,20)
    STEP(G,a,b,c,d,x[13],0xa9e3e905, 5) STEP(G,d,a,b,c,x[ 2],0xfcefa3f8, 9)
    STEP(G,c,d,a,b,x[ 7],0x676f02d9,14) STEP(G,b,c,d,a,x[12],0x8d2a4c8a,20)

    STEP(H,a,b,c,d,x[ 5],0xfffa3942, 4) STEP(H,d,a,b,c,x[ 8],0x8771f681,11)
    STEP(H,c,d,a,b,x[11],0x6d9d6122,16) STEP(H,b,c,d,a,x[14],0xfde5380c,23)
    STEP(H,a,b,c,d,x[ 1],0xa4beea44, 4) STEP(H,d,a,b,c,x[ 4],0x4bdecfa9,11)
    STEP(H,c,d,a,b,x[ 7],0xf6bb4b60,16) STEP(H,b,c,d,a,x[10],0xbebfbc70,23)
    STEP(H,a,b,c,d,x[13],0x289b7ec6, 4) STEP(H,d,a,b,c,x[ 0],0xeaa127fa,11)
    STEP(H,c,d,a,b,x[ 3],0xd4ef3085,16) STEP(H,b,c,d,a,x[ 6],0x04881d05,23)
    STEP(H,a,b,c,d,x[ 9],0xd9d4d039, 4) STEP(H,d,a,b,c,x[12],0xe6db99e5,11)
    STEP(H,c,d,a,b,x[15],0x1fa27cf8,16) STEP(H,b,c,d,a,x[ 2],0xc4ac5665,23)

    STEP(I,a,b,c,d,x[ 0],0xf4292244, 6) STEP(I,d,a,b,c,x[ 7],0x432aff97,10)
    STEP(I,c,d,a,b,x[14],0xab9423a7,15) STEP(I,b,c,d,a,x[ 5],0xfc93a039,21)
    STEP(I,a,b,c,d,x[12],0x655b59c3, 6) STEP(I,d,a,b,c,x[ 3],0x8f0ccc92,10)
    STEP(I,c,d,a,b,x[10],0xffeff47d,15) STEP(I,b,c,d,a,x[ 1],0x85845dd1,21)
    STEP(I,a,b,c,d,x[ 8],0x6fa87e4f, 6) STEP(I,d,a,b,c,x[15],0xfe2ce6e0,10)
    STEP(I,c,d,a,b,x[ 6],0xa3014314,15) STEP(I,b,c,d,a,x[13],0x4e0811a1,21)
    STEP(I,a,b,c,d,x[ 4],0xf7537e82, 6) STEP(I,d,a,b,c,x[11],0xbd3af235,10)
    STEP(I,c,d,a,b,x[ 2],0x2ad7d2bb,15) STEP(I,b,c,d,a,x[ 9],0xeb86d391,21)

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

void md5_init(md5_ctx *c)
{
    c->count[0] = c->count[1] = 0;
    c->state[0] = 0x67452301; c->state[1] = 0xefcdab89;
    c->state[2] = 0x98badcfe; c->state[3] = 0x10325476;
}

void md5_update(md5_ctx *c, const unsigned char *data, unsigned long len)
{
    unsigned int i, index, partLen;
    index = (unsigned int)((c->count[0] >> 3) & 0x3f);
    if ((c->count[0] += ((unsigned int)len << 3)) < ((unsigned int)len << 3))
        c->count[1]++;
    c->count[1] += ((unsigned int)len >> 29);
    partLen = 64 - index;
    if (len >= partLen) {
        memcpy(&c->buffer[index], data, partLen);
        md5_transform(c->state, c->buffer);
        for (i = partLen; i + 63 < len; i += 64)
            md5_transform(c->state, &data[i]);
        index = 0;
    } else i = 0;
    memcpy(&c->buffer[index], &data[i], (unsigned int)(len - i));
}

void md5_final(md5_ctx *c, unsigned char digest[16])
{
    static const unsigned char PAD[64] = {0x80};
    unsigned char bits[8];
    unsigned int index, padLen, i;
    for (i = 0; i < 2; i++) {
        bits[i*4]   = (unsigned char)(c->count[i] & 0xff);
        bits[i*4+1] = (unsigned char)((c->count[i] >> 8) & 0xff);
        bits[i*4+2] = (unsigned char)((c->count[i] >> 16) & 0xff);
        bits[i*4+3] = (unsigned char)((c->count[i] >> 24) & 0xff);
    }
    index = (unsigned int)((c->count[0] >> 3) & 0x3f);
    padLen = (index < 56) ? (56 - index) : (120 - index);
    md5_update(c, PAD, padLen);
    md5_update(c, bits, 8);
    for (i = 0; i < 4; i++) {
        digest[i*4]   = (unsigned char)(c->state[i] & 0xff);
        digest[i*4+1] = (unsigned char)((c->state[i] >> 8) & 0xff);
        digest[i*4+2] = (unsigned char)((c->state[i] >> 16) & 0xff);
        digest[i*4+3] = (unsigned char)((c->state[i] >> 24) & 0xff);
    }
}

void md5_hex(const unsigned char *data, unsigned long len, char out[33])
{
    static const char *hx = "0123456789abcdef";
    unsigned char d[16]; md5_ctx c; int i;
    md5_init(&c); md5_update(&c, data, len); md5_final(&c, d);
    for (i = 0; i < 16; i++) { out[i*2] = hx[d[i] >> 4]; out[i*2+1] = hx[d[i] & 15]; }
    out[32] = 0;
}
