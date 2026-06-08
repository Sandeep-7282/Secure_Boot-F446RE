#include "main.h"
#include "uart.h"
#include "boot_verify.h"
#include "app_jump.h"

#define APP_BASE 0x08020000UL

int main(void)
{
    uart_init();
    uart_send_str("=== Secure Bootloader ===\r\n");
    uart_send_str("[BOOT] Starting verification...\r\n");

    if (boot_verify_application() != BOOT_VERIFY_OK) {
        uart_send_str("[BOOT] Verification failed. Halting.\r\n");
        while (1);
    }

    uart_send_str("[BOOT] Jumping to application...\r\n");
    boot_jump_to_application(APP_BASE);

    while (1);
}
