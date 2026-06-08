/*
 * sha256.h
 *
 *  Created on: Jun 6, 2026
 *      Author: sande
 */

#ifndef INC_SHA256_H_
#define INC_SHA256_H_

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t  data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} SHA256_CTX;

void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const uint8_t *data, size_t len);
void sha256_final(SHA256_CTX *ctx, uint8_t *hash);
void sha256_compute(const uint8_t *data, size_t len, uint8_t *hash);

#endif /* INC_SHA256_H_ */
