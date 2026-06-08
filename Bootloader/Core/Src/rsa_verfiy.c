#include "rsa_verify.h"
#include "uart.h"
#include <string.h>

#define BN_SIZE 256

static void bn_zero(uint8_t *a)             { memset(a, 0, BN_SIZE); }
static void bn_copy(uint8_t *d, const uint8_t *s) { memcpy(d, s, BN_SIZE); }

static void bn_one(uint8_t *a)
{
    bn_zero(a);
    a[BN_SIZE - 1] = 1;
}

static int bn_cmp(const uint8_t *a, const uint8_t *b)
{
    for (int i = 0; i < BN_SIZE; i++) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return  1;
    }
    return 0;
}

static void bn_addmod(uint8_t *result,
                      const uint8_t *a,
                      const uint8_t *b,
                      const uint8_t *n)
{
    uint16_t carry = 0;
    uint8_t  tmp[BN_SIZE];

    for (int i = BN_SIZE - 1; i >= 0; i--) {
        uint16_t sum = (uint16_t)a[i] + b[i] + carry;
        tmp[i] = (uint8_t)(sum & 0xFF);
        carry  = sum >> 8;
    }

    if (carry || bn_cmp(tmp, n) >= 0) {
        uint16_t borrow = 0;
        for (int i = BN_SIZE - 1; i >= 0; i--) {
            int16_t diff = (int16_t)tmp[i] - n[i] - borrow;
            if (diff < 0) { diff += 256; borrow = 1; }
            else          { borrow = 0; }
            result[i] = (uint8_t)diff;
        }
    } else {
        bn_copy(result, tmp);
    }
}

static void bn_mulmod(uint8_t *result,
                      const uint8_t *a,
                      const uint8_t *b,
                      const uint8_t *n)
{
    uint8_t R[BN_SIZE];
    uint8_t A[BN_SIZE];
    bn_zero(R);
    bn_copy(A, a);

    for (int byte_i = BN_SIZE - 1; byte_i >= 0; byte_i--) {
        for (int bit = 0; bit <= 7; bit++) {       /* ← FIX: LSB first */
            if ((b[byte_i] >> bit) & 1)
                bn_addmod(R, R, A, n);
            bn_addmod(A, A, A, n);
        }
    }
    bn_copy(result, R);
}

static void bn_powmod(uint8_t *result,
                      const uint8_t *base,
                      uint32_t       exp,
                      const uint8_t *n)
{
    uint8_t R[BN_SIZE];
    uint8_t B[BN_SIZE];
    bn_one(R);
    bn_copy(B, base);

    while (exp > 0) {
        if (exp & 1)
            bn_mulmod(R, R, B, n);
        bn_mulmod(B, B, B, n);
        exp >>= 1;
    }
    bn_copy(result, R);
}

/*
 * Correct PKCS#1 v1.5 DigestInfo prefix for SHA-256 (19 bytes)
 * Does NOT include the 0x00 separator — checked separately
 * DOES include 0x20 (OCTET STRING length) at end
 */
static const uint8_t PKCS1_SHA256_PREFIX[19] = {
    0x30, 0x31, 0x30, 0x0d, 0x06, 0x09,
    0x60, 0x86, 0x48, 0x01, 0x65, 0x03,
    0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20  /* ← 0x20 included */
};

RSA_Status rsa_verify(const uint8_t *signature,
                      const uint8_t *hash,
                      const uint8_t *modulus,
                      uint32_t       exponent)
{
    uint8_t decrypted[BN_SIZE];

    bn_powmod(decrypted, signature, exponent, modulus);

    /* PKCS#1 v1.5 structure:
     * 0x00 | 0x01 | [0xFF...] | 0x00 | DigestInfo(19B) | Hash(32B) */

    if (decrypted[0] != 0x00) return RSA_VERIFY_FAIL;
    if (decrypted[1] != 0x01) return RSA_VERIFY_FAIL;

    /* Find 0x00 separator */
    int sep = -1;
    for (int i = 2; i < BN_SIZE - 32; i++) {
        if (decrypted[i] == 0x00) { sep = i; break; }
        if (decrypted[i] != 0xFF) return RSA_VERIFY_FAIL;
    }
    if (sep < 10) return RSA_VERIFY_FAIL;

    /* Check DigestInfo prefix starting at sep+1 */
    if (memcmp(&decrypted[sep + 1],                /* ← FIX: sep+1 not sep */
               PKCS1_SHA256_PREFIX,
               sizeof(PKCS1_SHA256_PREFIX)) != 0)
        return RSA_VERIFY_FAIL;

    /* Hash starts after separator(1) + DigestInfo prefix(19) */
    int hash_offset = sep + 1 + sizeof(PKCS1_SHA256_PREFIX); /* ← FIX: sep+20 */
    if (memcmp(&decrypted[hash_offset], hash, 32) != 0)
        return RSA_VERIFY_FAIL;

    return RSA_VERIFY_OK;
}
