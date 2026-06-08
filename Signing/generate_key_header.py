# save as generate_key_header.py
import subprocess

result = subprocess.run(
    ["openssl", "rsa", "-pubin", "-in", "public_key.pem", "-noout", "-modulus"],
    capture_output=True, text=True
)

modulus_hex = result.stdout.strip().replace("Modulus=", "")

# Remove leading 00 if present (openssl padding byte)
if modulus_hex[:2] == "00":
    modulus_hex = modulus_hex[2:]

bytes_list = [modulus_hex[i:i+2] for i in range(0, len(modulus_hex), 2)]

print("#ifndef PUBLIC_KEY_H")
print("#define PUBLIC_KEY_H")
print("")
print("#include <stdint.h>")
print("")
print(f"#define RSA_KEY_SIZE_BYTES 256")
print("")
print("static const uint32_t RSA_PUBLIC_EXPONENT = 65537UL;")
print("")
print("static const uint8_t RSA_PUBLIC_MODULUS[RSA_KEY_SIZE_BYTES] = {")

for i, b in enumerate(bytes_list):
    comma = "," if i < len(bytes_list) - 1 else ""
    end = "\n" if (i + 1) % 8 == 0 else ""
    print(f"    0x{b}{comma}", end=end)

print("\n};")
print("")
print("#endif /* PUBLIC_KEY_H */")
