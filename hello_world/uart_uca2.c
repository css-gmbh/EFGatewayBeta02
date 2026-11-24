/*
 * eUSCI_A2 UART — same baud/framing model as UCA0 (19200 8N1, IRQ TX/RX).
 */

#include <msp430.h>
#include <stdint.h>

#include "crc16_iso15693.h"
#include "uart_uca2.h"

/* Same TI UG settings as UCA0: 8 MHz SMCLK, 19200, UCOS16=1 */
#define UCA2_UCBRW    26U
#define UCA2_UCMCTLW  (UCOS16 | (0xB6U << 8))

#define UCA2_IDLE_TIMER_TICKS  50000U         /* ~50 ms at SMCLK/8 = 1 MHz */

static volatile uint8_t uca2_rx_buf[UCA2_RX_BUF_SIZE];
static volatile uint16_t uca2_rx_len;

static uint8_t uca2_tx_buf[UCA2_TX_BUF_SIZE];
static volatile uint16_t uca2_tx_len;
static volatile uint16_t uca2_tx_index;
static volatile uint8_t uca2_tx_active;

static void uca2_idle_timer_restart(void)
{
    TA2CTL = TASSEL__SMCLK | ID__8 | MC__UP | TACLR;
}

static void uca2_idle_timer_stop(void)
{
    TA2CTL = TASSEL__SMCLK | ID__8 | MC__STOP;
}

void uart_uca2_rx_frame_placeholder(volatile uint8_t *buf, uint16_t len)
{
    uint16_t crc = crc16_iso15693((const uint8_t *)buf, len);
    (void)crc;
    /* TODO: process UCA2 frame */
}

void uart_uca2_init(void)
{
    /* P5.4 = UCA2TXD, P5.5 = UCA2RXD (P5SEL1=0, P5SEL0=1) */
    P5SEL1 &= (uint8_t)~(BIT4 | BIT5);
    P5SEL0 |= (BIT4 | BIT5);

    UCA2CTLW0 = UCSWRST;
    UCA2CTLW0 |= UCSSEL__SMCLK;
    /* 8N1 defaults: UC7BIT=0, UCSPB=0, UCPEN=0 */
    UCA2BRW = UCA2_UCBRW;
    UCA2MCTLW = UCA2_UCMCTLW;
    UCA2CTLW0 &= (uint16_t)~UCSWRST;

    UCA2IE = UCRXIE;                          /* TX IRQ enabled by uart_uca2_send() */

    /* Idle framing timer (Timer_A2 CCR0) — stopped until first RX byte */
    TA2CTL = TASSEL__SMCLK | ID__8 | MC__STOP | TACLR;
    TA2EX0 = TAIDEX_0;
    TA2CCR0 = UCA2_IDLE_TIMER_TICKS - 1U;
    TA2CCTL0 = CCIE;

    uca2_rx_len = 0U;
    uca2_tx_active = 0U;
}

uint8_t uart_uca2_tx_busy(void)
{
    return uca2_tx_active;
}

uint8_t uart_uca2_send(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if ((data == 0) || (len == 0U) || (len > UCA2_TX_BUF_SIZE)) {
        return 0U;
    }

    __disable_interrupt();
    if (uca2_tx_active != 0U) {
        __enable_interrupt();
        return 0U;
    }

    for (i = 0U; i < len; i++) {
        uca2_tx_buf[i] = data[i];
    }
    uca2_tx_len = len;
    uca2_tx_index = 0U;
    uca2_tx_active = 1U;
    UCA2IE |= UCTXIE;
    __enable_interrupt();
    return 1U;
}

uint8_t uart_uca2_send_str(const char *s)
{
    uint16_t len = 0U;

    if (s == 0) {
        return 0U;
    }
    while (s[len] != '\0') {
        len++;
        if (len > UCA2_TX_BUF_SIZE) {
            return 0U;
        }
    }
    return uart_uca2_send((const uint8_t *)s, len);
}

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = EUSCI_A2_VECTOR
__interrupt void eusci_a2_isr(void)
#elif defined(__GNUC__)
void __attribute__((interrupt(EUSCI_A2_VECTOR))) eusci_a2_isr(void)
#else
#error "Unsupported compiler for EUSCI_A2 ISR"
#endif
{
    switch (UCA2IV) {
    case UCIV__NONE:
        break;

    case UCIV__UCRXIFG: {
        uint8_t c = (uint8_t)UCA2RXBUF;

        if (uca2_rx_len < UCA2_RX_BUF_SIZE) {
            uca2_rx_buf[uca2_rx_len] = c;
            uca2_rx_len++;
            uca2_idle_timer_restart();
        }
        break;
    }

    case UCIV__UCTXIFG:
        if (uca2_tx_index < uca2_tx_len) {
            UCA2TXBUF = uca2_tx_buf[uca2_tx_index];
            uca2_tx_index++;
        } else {
            UCA2IE &= (uint16_t)~UCTXIE;
            uca2_tx_active = 0U;
        }
        break;

    default:
        break;
    }
}

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = TIMER2_A0_VECTOR
__interrupt void timer2_a0_isr(void)
#elif defined(__GNUC__)
void __attribute__((interrupt(TIMER2_A0_VECTOR))) timer2_a0_isr(void)
#else
#error "Unsupported compiler for TIMER2_A0 ISR"
#endif
{
    uint16_t len;

    uca2_idle_timer_stop();

    len = uca2_rx_len;
    if (len > 0U) {
        uart_uca2_rx_frame_placeholder(uca2_rx_buf, len);
        uca2_rx_len = 0U;
    }
}
