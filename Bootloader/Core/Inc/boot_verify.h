/*
 * boot_verify.h
 *
 *  Created on: Jun 6, 2026
 *      Author: sande
 */

#ifndef INC_BOOT_VERIFY_H_
#define INC_BOOT_VERIFY_H_

#include <stdint.h>

typedef enum {
    BOOT_VERIFY_OK   = 0,
    BOOT_VERIFY_FAIL = 1
} BootVerifyStatus;

BootVerifyStatus boot_verify_application(void);

#endif /* INC_BOOT_VERIFY_H_ */
