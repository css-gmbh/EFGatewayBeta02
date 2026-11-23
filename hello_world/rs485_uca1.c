/*
 * eUSCI_A1 half-duplex RS-485 driver (interrupt TX/RX + DE/RE).
 */

#include <msp430.h>
#include <stdint.h>

#include "crc16_iso15693.h"
#include "rs485_uca1.h"

/* 115200 @ 8 MHz SMCLK, UCOS16=1: UCBRx=4, UCBRFx=5, UCBRSx=0x55 (TI UG) */
#define RS485_UCBRW    4U
#define RS485_UCMCTLW  (UCOS16 | (5U << 4) | (0x55U << 8))

#define RS485_IDLE_TIMER_TICKS  50000U        /* ~50 ms at SMCLK/8 = 1 MHz */

#define RS485_DE_BIT   BIT3                   /* P4.3 */
#define RS485_RE_BIT   BIT2                   /* P4.2 */

static volatile uint8_t rs485_rx_buf[RS485_RX_BUF_SIZE];
static volatile uint16_t rs485_rx_len;

static uint8_t rs485_tx_buf[RS485_TX_BUF_SIZE];
static volatile uint16_t rs485_tx_len;
static volatile uint16_t rs485_tx_index;
static volatile uint8_t rs485_tx_active;

/* DE high = drive bus; /RE low = receive enabled (MAX485-style) */
static void rs485_set_tx_mode(void)
{
    P4OUT |= RS485_DE_BIT;                    /* DE = 1 */
    P4OUT |= RS485_RE_BIT;                    /* /RE = 1 → receiver off */
}

static void rs485_set_rx_mode(void)
{
    P4OUT &= (uint8_t)~RS485_DE_BIT;          /* DE = 0 */
    P4OUT &= (uint8_t)~RS485_RE_BIT;          /* /RE = 0 → receiver on */
}

static void rs485_idle_timer_restart(void)
{
    TA1CTL = TASSEL__SMCLK | ID__8 | MC__UP | TACLR;
}

static void rs485_idle_timer_stop(void)
{
    TA1CTL = TASSEL__SMCLK | ID__8 | MC__STOP;
}

void rs485_rx_frame_placeholder(volatile uint8_t *buf, uint16_t len)
{
    uint16_t crc = crc16_iso15693((const uint8_t *)buf, len);
    (void)crc;
    /* TODO: process RS-485 frame */
}

void rs485_init(void)
{
    /* DE / /RE as GPIO outputs; start in receive */
    P4DIR |= (RS485_DE_BIT | RS485_RE_BIT);
    rs485_set_rx_mode();

    /* P2.5 = UCA1TXD, P2.6 = UCA1RXD (P2SEL1=1, P2SEL0=0) */
    P2SEL1 |= (BIT5 | BIT6);
    P2SEL0 &= (uint8_t)~(BIT5 | BIT6);

    UCA1CTLW0 = UCSWRST;
    UCA1CTLW0 |= UCSSEL__SMCLK;
    UCA1BRW = RS485_UCBRW;
    UCA1MCTLW = RS485_UCMCTLW;
    UCA1CTLW0 &= (uint16_t)~UCSWRST;

    UCA1IE = UCRXIE;                          /* TX IRQs enabled by rs485_send() */

    /* Idle framing timer (Timer_A1 CCR0) — stopped until first RX byte */
    TA1CTL = TASSEL__SMCLK | ID__8 | MC__STOP | TACLR;
    TA1EX0 = TAIDEX_0;
    TA1CCR0 = RS485_IDLE_TIMER_TICKS - 1U;
    TA1CCTL0 = CCIE;

    rs485_rx_len = 0U;
    rs485_tx_active = 0U;
}

uint8_t rs485_tx_busy(void)
{
    return rs485_tx_active;
}

uint8_t rs485_send(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if ((data == 0) || (len == 0U) || (len > RS485_TX_BUF_SIZE)) {
        return 0U;
    }

    __disable_interrupt();
    if (rs485_tx_active != 0U) {
        __enable_interrupt();
        return 0U;
    }

    for (i = 0U; i < len; i++) {
        rs485_tx_buf[i] = data[i];
    }
    rs485_tx_len = len;
    rs485_tx_index = 0U;
    rs485_tx_active = 1U;

    rs485_set_tx_mode();
    UCA1IFG &= (uint16_t)~UCTXCPTIFG;         /* clear stale complete flag */
    UCA1IE = (uint16_t)((UCA1IE & (uint16_t)~UCTXCPTIE) | UCRXIE | UCTXIE);
    __enable_interrupt();
    return 1U;
}

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = EUSCI_A1_VECTOR
__interrupt void eusci_a1_isr(void)
#elif defined(__GNUC__)
void __attribute__((interrupt(EUSCI_A1_VECTOR))) eusci_a1_isr(void)
#else
#error "Unsupported compiler for EUSCI_A1 ISR"
#endif
{
    switch (UCA1IV) {
    case UCIV__NONE:
        break;

    case UCIV__UCRXIFG: {
        uint8_t c = (uint8_t)UCA1RXBUF;

        /* Ignore RX while driving the bus (half-duplex echo/noise) */
        if (rs485_tx_active != 0U) {
            break;
        }
        if (rs485_rx_len < RS485_RX_BUF_SIZE) {
            rs485_rx_buf[rs485_rx_len] = c;
            rs485_rx_len++;
            rs485_idle_timer_restart();
        }
        break;
    }

    case UCIV__UCTXIFG:
        if (rs485_tx_index < rs485_tx_len) {
            UCA1TXBUF = rs485_tx_buf[rs485_tx_index];
            rs485_tx_index++;
        } else {
            /* All bytes accepted by TXBUF — wait for last bit shifted out */
            UCA1IE &= (uint16_t)~UCTXIE;
            UCA1IE |= UCTXCPTIE;
        }
        break;

    case UCIV__UCTXCPTIFG:
        UCA1IE &= (uint16_t)~UCTXCPTIE;
        UCA1IFG &= (uint16_t)~UCTXCPTIFG;
        rs485_set_rx_mode();
        rs485_tx_active = 0U;
        break;

    default:
        break;
    }
}

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = TIMER1_A0_VECTOR
__interrupt void timer1_a0_isr(void)
#elif defined(__GNUC__)
void __attribute__((interrupt(TIMER1_A0_VECTOR))) timer1_a0_isr(void)
#else
#error "Unsupported compiler for TIMER1_A0 ISR"
#endif
{
    uint16_t len;

    rs485_idle_timer_stop();

    len = rs485_rx_len;
    if (len > 0U) {
        rs485_rx_frame_placeholder(rs485_rx_buf, len);
        rs485_rx_len = 0U;
    }
}
