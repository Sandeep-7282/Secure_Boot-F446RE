# Secure-Bootloader
# STM32F446RE Secure Bootloader

A custom RSA-2048 + SHA-256 secure bootloader implemented from scratch in bare-metal C for the STM32F446RE microcontroller. No HAL security libraries. No external secure boot frameworks. Pure register-level implementation.

---

## Table of Contents

- [Overview](#overview)
- [Security Architecture](#security-architecture)
- [Flash Memory Layout](#flash-memory-layout)
- [Boot Flow](#boot-flow)
- [Project Structure](#project-structure)
- [Hardware Requirements](#hardware-requirements)
- [Linux Environment Setup](#linux-environment-setup)
- [Key Generation](#key-generation)
- [Building The Project](#building-the-project)
- [Signing Firmware](#signing-firmware)
- [Flashing Procedure](#flashing-procedure)
- [Attack Test Matrix](#attack-test-matrix)
- [Security Limitations](#security-limitations)

---

## Overview

This project implements a two-stage boot system on the STM32F446RE:

- **Stage 1 — Bootloader**: Starts at `0x08000000`. Performs cryptographic verification of the application image before allowing execution.
- **Stage 2 — Application**: Starts at `0x08020000`. Only executes if the bootloader passes RSA-2048 + SHA-256 signature verification.

The bootloader verifies that the application binary was signed by the holder of the RSA private key. The corresponding public key is compiled directly into the bootloader binary — an attacker cannot substitute their own key without replacing the bootloader itself.

---

## Security Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    THREAT MODEL                             │
│                                                             │
│  Attacker has:                                              │
│  ✓ Physical flash write access                              │
│  ✓ Ability to generate their own RSA key pairs              │
│  ✓ Ability to craft arbitrary firmware binaries             │
│                                                             │
│  Attacker cannot:                                           │
│  ✗ Access private key (stored offline, never on device)     │
│  ✗ Modify bootloader binary (protected by RDP Level 1)      │
│  ✗ Forge a valid signature without the private key          │
└─────────────────────────────────────────────────────────────┘
```

### Cryptographic Chain

```
Developer Machine (Linux)
│
├── private_key.pem  ← NEVER leaves this machine, NEVER in Git
│
│   [Signing Pipeline]
│   Application.bin
│       │
│       ▼
│   SHA-256 Hash (32 bytes)
│       │
│       ▼
│   RSA-2048 Sign with private_key.pem (PKCS#1 v1.5)
│       │
│       ▼
│   Signature (256 bytes)
│       │
│       ▼
│   metadata.bin → flashed to Sector 3 (0x0800C000)
│
STM32F446RE (Boot time)
│
├── public_key.h compiled into bootloader binary (Sector 0-2)
│
│   [Verification Pipeline - every reset]
│   Read BootMetadata_t from Sector 3
│       │
│       ├── Magic number check (0xB007CAFE)
│       ├── App address sanity check
│       ├── App size sanity check
│       │
│       ▼
│   Recompute SHA-256 of application region
│       │
│       ▼
│   RSA Verify: signature^65537 mod N = PKCS1(SHA256(app))
│       │
│       ├── PASS → Jump to application
│       └── FAIL → Infinite halt + UART error
```

### Why Public Key Lives In Bootloader Binary

The public key is a `const uint8_t` array in `public_key.h`, compiled into the `.rodata` section of the bootloader, placed in Sector 0-2 flash.

If the public key were stored in Sector 3 (metadata), an attacker could:
1. Generate their own RSA key pair
2. Write their public key to Sector 3
3. Sign malicious firmware with their private key
4. Bootloader verifies against attacker's public key — passes

By embedding the public key in the bootloader binary, this attack is eliminated. Replacing the public key requires replacing the bootloader, which requires JTAG access — blocked by RDP Level 1 in production.

---

## Flash Memory Layout

```
STM32F446RE — 512KB Flash (0x08000000 to 0x0807FFFF)

┌─────────────────────────────────────────────────────────────┐
│  Address Range          │ Size  │ Sector  │ Owner           │
├─────────────────────────┼───────┼─────────┼─────────────────┤
│  0x08000000 - 0x0800BFFF│  48KB │  0,1,2  │ Bootloader      │
│                         │       │         │ ├─ Vector table  │
│                         │       │         │ ├─ Boot logic    │
│                         │       │         │ ├─ SHA-256 engine│
│                         │       │         │ ├─ RSA verify    │
│                         │       │         │ └─ Public key    │
├─────────────────────────┼───────┼─────────┼─────────────────┤
│  0x0800C000 - 0x0800FFFF│  16KB │    3    │ Metadata        │
│                         │       │         │ ├─ Magic number  │
│                         │       │         │ ├─ App address   │
│                         │       │         │ ├─ App size      │
│                         │       │         │ ├─ App version   │
│                         │       │         │ └─ RSA signature │
├─────────────────────────┼───────┼─────────┼─────────────────┤
│  0x08010000 - 0x0801FFFF│  64KB │    4    │ Reserved (OTA)  │
├─────────────────────────┼───────┼─────────┼─────────────────┤
│  0x08020000 - 0x0807FFFF│ 384KB │  5,6,7  │ Application     │
│                         │       │         │ ├─ Vector table  │
│                         │       │         │ └─ App code      │
└─────────────────────────┴───────┴─────────┴─────────────────┘

RAM Layout (128KB at 0x20000000)
┌─────────────────────────────────────────────────────────────┐
│  0x20000000            │ Bootloader .data + .bss            │
│  ...                   │ Application .data + .bss           │
│  0x20020000            │ Top of RAM — initial MSP (_estack) │
└─────────────────────────────────────────────────────────────┘
```

---

## Boot Flow

```
                    MCU RESET
                        │
                        ▼
            ┌───────────────────────┐
            │  CPU fetches from     │
            │  0x08000000           │
            │  [0] → MSP value      │
            │  [4] → Reset_Handler  │
            └───────────┬───────────┘
                        │
                        ▼
            ┌───────────────────────┐
            │  Bootloader           │
            │  Reset_Handler        │
            │  ├─ Copy .data        │
            │  ├─ Zero .bss         │
            │  └─ Call main()       │
            └───────────┬───────────┘
                        │
                        ▼
            ┌───────────────────────┐
            │  uart_init()          │
            │  UART logs active     │
            └───────────┬───────────┘
                        │
                        ▼
            ┌───────────────────────┐
            │  Read BootMetadata_t  │
            │  from 0x0800C000      │
            └───────────┬───────────┘
                        │
                        ▼
            ┌───────────────────────┐
            │  Magic == 0xB007CAFE? │
            ├── NO ─────────────────┼──→ HALT + UART error
            └───────────┬───────────┘
                        │ YES
                        ▼
            ┌───────────────────────┐
            │  Address == 0x08020000│
            │  Size in valid range? │
            ├── NO ─────────────────┼──→ HALT + UART error
            └───────────┬───────────┘
                        │ YES
                        ▼
            ┌───────────────────────┐
            │  SHA-256(app_bytes)   │
            │  → computed_hash      │
            └───────────┬───────────┘
                        │
                        ▼
            ┌───────────────────────┐
            │  RSA Verify:          │
            │  sig^e mod N          │
            │  == PKCS1(hash)?      │
            ├── NO ─────────────────┼──→ HALT + UART error
            └───────────┬───────────┘
                        │ YES
                        ▼
            ┌───────────────────────┐
            │  Disable IRQs         │
            │  Clear SysTick        │
            │  Clear NVIC           │
            │  Set MSP from app[0]  │
            │  Set VTOR = 0x08020000│
            │  DSB + ISB            │
            │  Enable IRQs          │
            └───────────┬───────────┘
                        │
                        ▼
            ┌───────────────────────┐
            │  Jump to App          │
            │  Reset_Handler        │
            │  at app[4]            │
            └───────────┬───────────┘
                        │
                        ▼
            ┌───────────────────────┐
            │  Application runs     │
            │  VTOR → 0x08020000    │
            │  IRQs → App handlers  │
            └───────────────────────┘
```

---

## BootMetadata_t Structure

```c
typedef struct {
    uint32_t magic;              // 0xB007CAFE — sanity check
    uint32_t app_start_address;  // 0x08020000 — verified by bootloader
    uint32_t app_size;           // exact byte count of application binary
    uint32_t app_version;        // reserved for rollback protection
    uint8_t  signature[256];     // RSA-2048 PKCS#1 v1.5 signature
} __attribute__((packed)) BootMetadata_t; // total: 272 bytes
```

`__attribute__((packed))` is mandatory — this struct is cast directly from flash memory and must match the Python `struct.pack` layout byte-for-byte with no compiler padding.

---

## Project Structure

```
SecureBootloader/
│
├── Bootloader_F446/                  ← STM32CubeIDE Project 1
│   ├── Core/
│   │   ├── Inc/
│   │   │   ├── main.h
│   │   │   ├── uart.h
│   │   │   ├── app_jump.h
│   │   │   ├── boot_verify.h
│   │   │   ├── boot_metadata.h      ← shared struct definition
│   │   │   ├── sha256.h
│   │   │   ├── rsa_verify.h
│   │   │   └── public_key.h         ← generated from public_key.pem
│   │   └── Src/
│   │       ├── main.c
│   │       ├── uart.c
│   │       ├── app_jump.c
│   │       ├── boot_verify.c
│   │       ├── sha256.c
│   │       └── rsa_verify.c
│   └── LinkerScript.ld              ← FLASH origin 0x08000000, length 48K
│
├── Application_F446/                 ← STM32CubeIDE Project 2
│   ├── Core/
│   │   ├── Inc/
│   │   │   ├── main.h
│   │   │   ├── uart.h
│   │   │   └── boot_metadata.h      ← same shared struct
│   │   └── Src/
│   │       ├── main.c
│   │       └── uart.c
│   └── LinkerScript.ld              ← FLASH origin 0x08020000, length 384K
│
├── signing/                          ← Linux signing toolchain
│   ├── sign_firmware.py             ← signing script
│   ├── generate_key_header.py       ← converts PEM to C header
│   └── public_key.pem               ← public key only (safe to commit)
│
└── README.md
```

**What must NEVER be in this repository:**
```
private_key.pem    ← if this leaks, attacker becomes an authorized signer
metadata.bin       ← build artifact, regenerated each release
*.bin              ← build artifacts
```

Add to `.gitignore`:
```
private_key.pem
metadata.bin
*.bin
*.elf
*.map
Debug/
```

---

## Hardware Requirements

| Component | Details |
|-----------|---------|
| MCU Board | STM32F446RE Nucleo-64 |
| Debugger | ST-Link v2 (built into Nucleo) |
| IDE | STM32CubeIDE |
| Flash Tool | STM32CubeProgrammer |
| Terminal | Any serial monitor at 115200 baud |
| USB Cable | USB-A to Mini-B (Nucleo USB port) |

UART debug output uses USART2 on PA2 (TX) / PA3 (RX) — routed to ST-Link virtual COM port on Nucleo. No external USB-UART adapter needed.

---

## Linux Environment Setup

A Linux environment is required for key generation and firmware signing. WSL2 on Windows is sufficient.

### Option A — WSL2 on Windows

```bash
# Install WSL2 (run in Windows PowerShell as Administrator)
wsl --install -d Ubuntu

# Restart, then open Ubuntu terminal
```

### Option B — Native Linux / VM

Any Ubuntu 20.04+ or Debian-based distro works.

### Install Required Packages

```bash
sudo apt update
sudo apt install -y openssl python3 python3-pip build-essential

pip3 install cryptography
```

### Verify Installation

```bash
openssl version      # should print OpenSSL 1.1.x or 3.x
python3 --version    # should print Python 3.8+
python3 -c "from cryptography.hazmat.primitives import hashes; print('OK')"
```

---

## Key Generation

This is a one-time setup per deployment. Keys must be regenerated if the private key is ever compromised.

### Step 1 — Generate RSA-2048 Key Pair

```bash
cd signing/

# Generate 2048-bit RSA private key
openssl genrsa -out private_key.pem 2048

# Extract public key
openssl rsa -in private_key.pem -pubout -out public_key.pem

# Verify
ls -la private_key.pem public_key.pem
```

### Step 2 — Generate C Header From Public Key

```bash
python3 generate_key_header.py > ../Bootloader_F446/Core/Inc/public_key.h

# Verify — should show 256 bytes in array
cat ../Bootloader_F446/Core/Inc/public_key.h
```

The `generate_key_header.py` script:

```python
import subprocess

result = subprocess.run(
    ["openssl", "rsa", "-pubin", "-in", "public_key.pem", "-noout", "-modulus"],
    capture_output=True, text=True
)

modulus_hex = result.stdout.strip().replace("Modulus=", "")
if modulus_hex[:2] == "00":
    modulus_hex = modulus_hex[2:]

bytes_list = [modulus_hex[i:i+2] for i in range(0, len(modulus_hex), 2)]

print("#ifndef PUBLIC_KEY_H")
print("#define PUBLIC_KEY_H")
print("#include <stdint.h>")
print("#define RSA_KEY_SIZE_BYTES 256")
print("static const uint32_t RSA_PUBLIC_EXPONENT = 65537UL;")
print("static const uint8_t RSA_PUBLIC_MODULUS[RSA_KEY_SIZE_BYTES] = {")
for i, b in enumerate(bytes_list):
    comma = "," if i < len(bytes_list) - 1 else ""
    end = "\n" if (i + 1) % 8 == 0 else ""
    print(f"    0x{b}{comma}", end=end)
print("\n};")
print("#endif /* PUBLIC_KEY_H */")
```

### Step 3 — Rebuild Bootloader

After generating `public_key.h`, rebuild the bootloader in STM32CubeIDE. The new public key is now compiled into the binary. **This is the only step that requires reflashing the bootloader.**

---

## Building The Project

### Bootloader

1. Open `Bootloader_F446` in STM32CubeIDE
2. Verify `LinkerScript.ld` has `FLASH ORIGIN = 0x08000000, LENGTH = 48K`
3. Build → `Debug/Bootloader_F446.bin`

### Application

1. Open `Application_F446` in STM32CubeIDE
2. Verify `LinkerScript.ld` has `FLASH ORIGIN = 0x08020000, LENGTH = 384K`
3. Build → `Debug/Application_F446.bin`

---

## Signing Firmware

After building the application, sign it on Linux:

```bash
cd signing/

# Copy application binary from Windows to Linux
# (if using WSL2, Windows drives are at /mnt/c/)
cp /mnt/c/Users/<user>/STM32CubeIDE/workspace/Application_F446/Debug/Application_F446.bin .

# Sign
python3 sign_firmware.py Application_F446.bin private_key.pem metadata.bin

# Expected output:
# App size     : XXXXX bytes
# SHA256       : <64 hex chars>
# Signature    : <first 32 chars>...
# Sig length   : 256 bytes
# Metadata written to metadata.bin (272 bytes)

# Verify signature independently (sanity check)
openssl dgst -sha256 -verify public_key.pem \
    -signature <(python3 -c "
import struct, sys
with open('metadata.bin','rb') as f: data = f.read()
sig = struct.unpack_from('256s', data, 16)[0]
sys.stdout.buffer.write(sig)
") Application_F446.bin
# Should print: Verified OK
```

The `sign_firmware.py` script:

```python
#!/usr/bin/env python3
import struct, sys, hashlib
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.backends import default_backend

BOOT_MAGIC     = 0xB007CAFE
APP_START_ADDR = 0x08020000
APP_VERSION    = 1

def sign_firmware(bin_path, key_path, out_path):
    with open(bin_path, "rb") as f:
        app_data = f.read()
    app_size = len(app_data)

    sha256_hash = hashlib.sha256(app_data).digest()
    print(f"App size     : {app_size} bytes")
    print(f"SHA256       : {sha256_hash.hex()}")

    with open(key_path, "rb") as f:
        private_key = serialization.load_pem_private_key(
            f.read(), password=None, backend=default_backend()
        )

    # Pass raw app_data — library hashes internally with SHA256
    signature = private_key.sign(app_data, padding.PKCS1v15(), hashes.SHA256())
    print(f"Signature    : {signature.hex()[:32]}...")
    print(f"Sig length   : {len(signature)} bytes")

    metadata = struct.pack('<IIII256s',
        BOOT_MAGIC, APP_START_ADDR, app_size, APP_VERSION, signature)

    with open(out_path, "wb") as f:
        f.write(metadata)
    print(f"Metadata written to {out_path} ({len(metadata)} bytes)")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: python3 sign_firmware.py <app.bin> <private_key.pem> <metadata.bin>")
        sys.exit(1)
    sign_firmware(sys.argv[1], sys.argv[2], sys.argv[3])
```

---

## Flashing Procedure

Use STM32CubeProgrammer. Flash in this exact order. Use **Start Address** field — do not use full-chip erase.

### Step 1 — Flash Bootloader

```
File    : Bootloader_F446/Debug/Bootloader_F446.bin
Address : 0x08000000
```

### Step 2 — Flash Application

```
File    : Application_F446/Debug/Application_F446.bin
Address : 0x08020000
```

### Step 3 — Flash Metadata

```
File    : signing/metadata.bin
Address : 0x0800C000
```

### Step 4 — Verify On Terminal

Open serial terminal (PuTTY, TeraTerm, minicom):
```
Port     : STLink Virtual COM Port
Baud     : 115200
Data     : 8 bits
Stop     : 1 bit
Parity   : None
```

Press reset on Nucleo board. Expected output:
```
=== Secure Bootloader ===
[BOOT] Starting verification...
[BOOT] Magic OK
[BOOT] Metadata sanity OK
[BOOT] SHA256 computed
[BOOT] Signature verification PASSED
[BOOT] Jumping to application...
Welcome to Application v1.0
```

---

## Attack Test Matrix

Each test verifies a specific security guarantee. Run all tests after any modification.

| Test | Action | Expected Result |
|------|--------|----------------|
| 1 — Valid image | Normal flash, valid metadata | Boot succeeds |
| 2 — Corrupted app | Flip byte in app binary, keep old metadata | SHA256 mismatch → HALT |
| 3 — Bad signature | Overwrite signature bytes in metadata.bin with garbage | RSA FAIL → HALT |
| 4 — Blank metadata | Erase Sector 3 only, no metadata flashed | Magic FAIL → HALT |
| 5 — Attacker key | Generate new key pair, sign app with attacker private key | RSA FAIL → HALT |
| 6 — Malicious firmware | Different app binary signed with attacker key | RSA FAIL → HALT |
| 7 — Size manipulation | Edit app_size field in metadata.bin | RSA FAIL → HALT |
| 8 — Wrong app address | Edit app_start_address in metadata.bin | Address sanity FAIL → HALT |
| 9 — Replay attack | Old metadata + new app binary | SHA256 mismatch → HALT |

Test 5 and Test 6 are the most important — they validate the core threat model.

---

## Security Limitations

Honest assessment of what this implementation does and does not protect against:

**Protected:**
- Unsigned firmware cannot boot
- Tampered firmware cannot boot
- Attacker-generated keys cannot produce valid signatures
- Missing or corrupt metadata halts boot

**Not protected (known limitations):**
- **Version rollback**: An older valid firmware + its valid metadata will still boot. Version field enforcement is not implemented.
- **JTAG attack**: Without RDP Level 1 set via option bytes, an attacker with physical access and JTAG can read flash, extract the public key context, or overwrite the bootloader. Set RDP Level 1 in production.
- **Side-channel attacks**: Software RSA modular exponentiation is not constant-time. Timing or power analysis attacks are theoretically possible in a lab environment.
- **Fault injection**: Hardware voltage glitching or clock glitching attacks are not mitigated.

**To harden for production:**
```bash
# Set RDP Level 1 via STM32CubeProgrammer
# Option Bytes → RDP → Level 1
# WARNING: This permanently disables JTAG readback
# Level 2 is irreversible and bricks JTAG forever — do not use Level 2
```

---

## Key Security Design Decisions

| Decision | Reason |
|----------|--------|
| RSA-2048 over ECDSA | Simpler mental model for PKCS#1 v1.5, well-understood padding, straightforward bignum implementation |
| Public key in bootloader binary | Prevents attacker from substituting their own public key in writable flash |
| SHA-256 before RSA sign | RSA-2048 can only operate on ≤256 bytes; SHA-256 reduces arbitrary-size binary to 32 bytes |
| PKCS#1 v1.5 padding | Standardized, verifiable structure; raw RSA without padding is insecure |
| Magic number check first | Fast-fail before expensive crypto operation if metadata sector is blank |
| `__attribute__((packed))` on metadata struct | Guarantees byte-exact layout match between Python struct.pack and C memory cast |
| Exponent = 65537 | Only 2 set bits in binary (10000000000000001) — minimizes square-and-multiply operations; large enough to resist small-exponent attacks |

---

*Built on STM32F446RE Nucleo-64. Tested with STM32CubeIDE and STM32CubeProgrammer.*
