/*
 * rsa_verify.h
 *
 *  Created on: Jun 6, 2026
 *      Author: sande
 */

#ifndef INC_RSA_VERIFY_H_
#define INC_RSA_VERIFY_H_

#include <stdint.h>

#define RSA_KEY_SIZE_BYTES  256

typedef enum {
    RSA_VERIFY_OK   = 0,
    RSA_VERIFY_FAIL = 1
} RSA_Status;

RSA_Status rsa_verify(const uint8_t *signature,
                      const uint8_t *hash,
                      const uint8_t *modulus,
                      uint32_t       exponent);

#endif /* INC_RSA_VERIFY_H_ */
