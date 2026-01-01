#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* include the implementation directly so we can call `pseudo_hash` */
#include "../praxis2/util.c"

static void print_hex(const unsigned char *buf, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        printf("%02X", buf[i]);
        if (i + 1 < len) putchar(' ');
    }
}

int main(void) {
    struct { const unsigned char *buf; size_t len; const char *name; } tests[] = {
        {(const unsigned char*)"", 0, "empty"},
        {(const unsigned char*)"A", 1, "single byte"},
        {(const unsigned char*)"abc", 3, "3 bytes"},
        {(const unsigned char*)"123456789", 9, "9 bytes (odd)"},
    };

    for (size_t t = 0; t < sizeof tests / sizeof tests[0]; ++t) {
        const unsigned char *buf = tests[t].buf;
        size_t len = tests[t].len;

        printf("--- Test: %s (len=%zu) ---\n", tests[t].name, len);
        printf("input bytes: "); print_hex(buf, len); printf("\n");

        /* show padded buffer (pseudo_hash pads one zero byte when length is odd) */
        size_t pad_len = (len % 2) ? (len + 1) : len;
        unsigned char *pad = calloc(pad_len, 1);
        if (!pad) { perror("calloc"); return 1; }
        memcpy(pad, buf, len);
        printf("padded bytes: "); print_hex(pad, pad_len); printf("\n");

        /* show per-16bit word values and the running 32-bit sum */
        uint32_t sum = 0;
        for (size_t i = 0; i < pad_len; i += 2) {
            uint16_t word = (uint16_t)(pad[i] << 8) | pad[i+1];
            printf("word[%zu]: 0x%04X (%u)\n", i/2, word, (unsigned)word);
            sum += word;
            printf(" running sum: 0x%08X (%u)\n", sum, (unsigned)sum);
        }

        uint32_t folded = (sum & 0xFFFF) + (sum >> 16);
        printf("after fold: 0x%04X (%u)\n", (unsigned)(folded & 0xFFFF), (unsigned)folded);
        uint16_t final_hash = (uint16_t) ~folded;
        printf("computed (manual) final: 0x%04X (%u)\n", (unsigned)final_hash, (unsigned)final_hash);

        /* call the real function to confirm */
        uint16_t h = pseudo_hash(buf, len);
        printf("pseudo_hash() returned: 0x%04X (%u)\n", (unsigned)h, (unsigned)h);

        if (h != final_hash) printf("MISMATCH!\n");

        free(pad);
        printf("\n");
    }

    return 0;
}
