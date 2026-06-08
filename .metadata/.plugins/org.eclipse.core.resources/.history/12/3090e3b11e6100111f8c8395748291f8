/*
 * uart.c
 *
 *  Minimal blocking UART for bootloader use.
 *
 */

#include "stm32f446xx.h"
#include "uart.h"

static void uart_gpio_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    /* PA2 = TX, PA3 = RX (AF7) */
    GPIOA->MODER &= ~((3U << (2 * 2)) | (3U << (3 * 2)));
    GPIOA->MODER |=  ((2U << (2 * 2)) | (2U << (3 * 2)));

    GPIOA->AFR[0] &= ~((0xFU << (4 * 2)) | (0xFU << (4 * 3)));
    GPIOA->AFR[0] |=  ((7U  << (4 * 2)) | (7U  << (4 * 3)));
}

void uart_init(void)
{
    uart_gpio_init();

    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /*
     * Bootloader clock:
     *  - HSI @ 16 MHz
     *  - APB1 prescaler = 1
     */
    USART2->BRR = 16000000U / 115200U;

    USART2->CR1 = USART_CR1_TE | USART_CR1_RE;
    USART2->CR1 |= USART_CR1_UE;
}

void uart_putc(char c)
{
    while (!(USART2->SR & USART_SR_TXE));
    USART2->DR = (uint8_t)c;
}

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

char uart_getc(void)
{
    while (!(USART2->SR & USART_SR_RXNE));
    return (char)(USART2->DR & 0xFF);
}
