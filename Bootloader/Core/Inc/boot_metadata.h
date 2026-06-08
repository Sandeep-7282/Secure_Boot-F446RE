/*
 * boot_metadata.h
 *
 *  Created on: Jun 6, 2026
 *      Author: sande
 */

#ifndef INC_BOOT_METADATA_H_
#define INC_BOOT_METADATA_H_

#include <stdint.h>

#define BOOT_MAGIC  0xB007CAFE

typedef struct {
    uint32_t magic;                  // 4 bytes  — must equal BOOT_MAGIC
    uint32_t app_start_address;      // 4 bytes  — 0x08020000
    uint32_t app_size;               // 4 bytes  — exact application byte count
    uint32_t app_version;            // 4 bytes  — rollback protection later
    uint8_t  signature[256];         // 256 bytes — RSA-2048 signature
} __attribute__((packed)) BootMetadata_t;  // total: 272 bytes

#endif /* INC_BOOT_METADATA_H_ */
