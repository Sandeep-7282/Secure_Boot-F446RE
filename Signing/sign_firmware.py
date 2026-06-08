#!/usr/bin/env python3
import struct, sys, hashlib
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.backends import default_backend

BOOT_MAGIC       = 0xB007CAFE
APP_START_ADDR   = 0x08020000
APP_VERSION      = 1

def sign_firmware(bin_path, key_path, out_path):
    # Read application binary
    with open(bin_path, "rb") as f:
        app_data = f.read()
    app_size = len(app_data)

    # Print hash for debug reference
    sha256_hash = hashlib.sha256(app_data).digest()
    print(f"App size     : {app_size} bytes")
    print(f"SHA256       : {sha256_hash.hex()}")

    # Load private key
    with open(key_path, "rb") as f:
        private_key = serialization.load_pem_private_key(
            f.read(), password=None, backend=default_backend()
        )

    # Sign raw app_data directly — let cryptography library hash internally
    # DO NOT pass pre-hashed data here
    signature = private_key.sign(
        app_data,           # ← raw binary, not the hash
        padding.PKCS1v15(),
        hashes.SHA256()     # ← library computes SHA256(app_data) internally
    )

    print(f"Signature    : {signature.hex()[:32]}...")
    print(f"Sig length   : {len(signature)} bytes")

    # Pack BootMetadata_t
    metadata = struct.pack(
        '<IIII256s',
        BOOT_MAGIC,
        APP_START_ADDR,
        app_size,
        APP_VERSION,
        signature
    )

    with open(out_path, "wb") as f:
        f.write(metadata)
    print(f"Metadata written to {out_path} ({len(metadata)} bytes)")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: python3 sign_firmware.py <app.bin> <private_key.pem> <metadata.bin>")
        sys.exit(1)
    sign_firmware(sys.argv[1], sys.argv[2], sys.argv[3])
