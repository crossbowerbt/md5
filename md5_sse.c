/*
 * Simple MD5 cracker implementation
 *
 * Compile with: gcc -o md5_sse -msse -mmmx -msse2 -O3 md5_sse.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <xmmintrin.h>

// starting values
#define H0 0x67452301
#define H1 0xefcdab89
#define H2 0x98badcfe
#define H3 0x10325476

// r specifies the per-round shift amounts
const uint32_t r[] = { 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                       5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
                       4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                       6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21 };

__m128i rv[sizeof(r)/sizeof(*r)]; // optimized for SSE

// Use binary integer part of the sines of integers (Radians) as constants:
const uint32_t k[] = { 0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
                       0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
                       0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
                       0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
                       0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
                       0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
                       0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
                       0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
                       0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
                       0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
                       0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
                       0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
                       0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
                       0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
                       0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
                       0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 };

__m128i kv[sizeof(k)/sizeof(*k)]; // optimized for SSE

// prepare optimized tables
void prepare_tables() {
    int i;

    for(i=0; i<(sizeof(rv)/sizeof(*rv)); i++) {
        rv[i] = _mm_set_epi32(r[i], r[i], r[i], r[i]);
    }

    for(i=0; i<(sizeof(kv)/sizeof(*kv)); i++) {
        kv[i] = _mm_set_epi32(k[i], k[i], k[i], k[i]);
    }
}

// print 128bit data
inline void print128v(__m128i x) {

    uint32_t res_x[4];

    _mm_store_si128((__m128i *) res_x, x);

    printf("%8.8x %8.8x %8.8x %8.8x\n", res_x[0],res_x[1],res_x[2],res_x[3]);
}

// leftrotate function definition
inline __m128i leftrotate(__m128i a, __m128i f,  __m128i kv,  __m128i blockv, __m128i rv) {

    __m128i x = _mm_add_epi32(a, _mm_add_epi32(f, _mm_add_epi32(kv, blockv)));

    return _mm_or_si128( _mm_sll_epi32(x, rv),
                         _mm_srl_epi32(x, _mm_sub_epi32( _mm_set_epi32(32, 32, 32, 32), rv)));
}

// These var contains the hash to crack
uint32_t c0, c1, c2, c3;

// main cracking function
uint64_t md5(uint64_t start_word, uint64_t stop_word, size_t len) {

    // Message block
    __m128i msg_block[16];

    // Note: All variables are unsigned 32 bit and wrap modulo 2^32 when calculating

    // Pre-processing: adding a single 1 bit
    //append "1" bit to message
    /* Notice: the input bytes are considered as bits strings,
       where the first bit is the most significant bit of the byte.[37] */

    // Pre-processing: padding with zeros
    //append "0" bit until message length in bit ≡ 448 (mod 512)
    //append length mod (2 pow 64) to message

    memset(msg_block, 0, sizeof(msg_block));

    msg_block[14] = _mm_set_epi32(len*8, len*8, len*8, len*8); // note, we append the len in bits
                                                               // at the end of the buffer

    uint64_t curr_word;
    for(curr_word = start_word; curr_word <= stop_word; curr_word+=4) {

        if(curr_word % 0x1000000 == 0) {
            printf("%16.16llx\n", curr_word);
        }

        // Write word in the block
        msg_block[0] = _mm_set_epi32(curr_word, curr_word+1, curr_word+2, curr_word+3);
        // TODO: support words with length > 4

        // Append the "1" bit
        uint8_t *p = (uint8_t *) msg_block;
        p[len+12] = p[len+16] = p[len+20] = p[len+24] = 0x80;

#define DEBUG 0

#if DEBUG
        printf("\nWords: %llx, %llx, %llx, %llx, size = %u\n\nMessage block:\n", curr_word, curr_word+1, curr_word+2, curr_word+3, len);

        int j;
        for(j=0; j<sizeof(msg_block); j++) {
            printf("%2.2x", p[j]);
            if((j+1)%4==0)  printf(" ");
            if((j+1)%16==0) puts("");
        }

#endif

        // Initialize hash value for this chunk:
        __m128i a = _mm_set_epi32(H0, H0, H0, H0);
        __m128i b = _mm_set_epi32(H1, H1, H1, H1);
        __m128i c = _mm_set_epi32(H2, H2, H2, H2);
        __m128i d = _mm_set_epi32(H3, H3, H3, H3);

        // Main loops:

        int i=0;

        for(; i<16; i++) {

            __m128i f = _mm_xor_si128(d, _mm_and_si128(b, _mm_xor_si128(c, d)));
            int g = i;

            __m128i temp = d;
            d = c;
            c = b;
            b = _mm_add_epi32(b, leftrotate(a, f, kv[i], msg_block[g], rv[i]));
            a = temp;

        }

        for(; i<32; i++) {

            __m128i f = _mm_xor_si128(c, _mm_and_si128(d, _mm_xor_si128(b, c)));
            int g = (5*i + 1) % 16;

            __m128i temp = d;
            d = c;
            c = b;
            b = _mm_add_epi32(b, leftrotate(a, f, kv[i], msg_block[g], rv[i]));
            a = temp;

        }

        for(; i<48; i++) {

            __m128i f = _mm_xor_si128(b, _mm_xor_si128(c, d));
            int g = (3*i + 5) % 16;

            __m128i temp = d;
            d = c;
            c = b;
            b = _mm_add_epi32(b, leftrotate(a, f, kv[i], msg_block[g], rv[i]));
            a = temp;

        }

        for(; i<64; i++) {

            __m128i f = _mm_xor_si128(c, _mm_or_si128(b, _mm_xor_si128(d, _mm_set_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF))));
            int g = (7*i) % 16;

            __m128i temp = d;
            d = c;
            c = b;
            b = _mm_add_epi32(b, leftrotate(a, f, kv[i], msg_block[g], rv[i]));
            a = temp;

        }

        // Check hash

        uint32_t res_a[4];
        uint32_t res_b[4];
        uint32_t res_c[4];
        uint32_t res_d[4];

        _mm_store_si128((__m128i *) res_a, a);
        _mm_store_si128((__m128i *) res_b, b);
        _mm_store_si128((__m128i *) res_c, c);
        _mm_store_si128((__m128i *) res_d, d);

        int y;
        for(y=0; y<4; y++) {
            if(res_a[y] == c0-H0 && res_b[y] == c1-H1 && res_c[y] == c2-H2 && res_d[y] == c3-H3)
                return curr_word + 3 - y;
        }


    }

    return 0;
}

int main(int argc, char **argv) {

    if (argc < 4) {
        printf("usage: %s 'start word' 'stop word' 'len in bytes of the word' 'hash in 32bit chunks'\n", argv[0]);
        return 1;
    }

    // read the start word
    uint64_t start_word;
    sscanf(argv[1], "%llx", &start_word);

    // read the stop word
    uint64_t stop_word;
    sscanf(argv[2], "%llx", &stop_word);

    // read the len
    size_t len = atoi(argv[3]);

    // read the hash to crack
    sscanf(argv[4], "%x %x %x %x", &c0, &c1, &c2, &c3);
    c0 = htonl(c0);
    c1 = htonl(c1);
    c2 = htonl(c2);
    c3 = htonl(c3);

    // prepare optimized tables
    prepare_tables();

    // info
    printf("start cracking '%s', from %p to %p, using words of length %d\n", argv[4], start_word, stop_word, len);

    // crack the MD5 hash
    uint64_t word = md5(start_word, stop_word, len);

    // display result (output is in little-endian)
    uint8_t *p=(uint8_t *)&word;
    printf("Word = %2.2x%2.2x%2.2x%2.2x\n", p[0], p[1], p[2], p[3]);

    return 1;
}
