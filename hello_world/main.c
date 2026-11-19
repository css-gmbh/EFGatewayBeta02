/*
 * MSP430FR5962 hello world — P1.0 blink + eUSCI_A0 UART
 *
 * UART (UCA0): TX = P2.0, RX = P2.1, 19200 baud, 8N1
 * Clock: DCO / SMCLK = 8 MHz (TI recommended baud settings for 19200)
 *
 * RX is interrupt-driven into a linear 256-byte buffer starting at index 0.
 * Each received byte restarts Timer_A0 (~50 ms). When the idle timer expires,
 * the buffer pointer (and length) are passed to a placeholder, then RX resumes
 * at index 0.
 */

#include <msp430.h>
#include <stdint.h>

/* TI UG recommended for 8 MHz SMCLK, 19200 baud, oversampling (UCOS16=1):
 * UCBRx=26, UCBRFx=0, UCBRSx=0xB6
 */
#define UART_UCBRW   26U
#define UART_UCMCTLW (UCOS16 | (0xB6U << 8))

#define RX_BUF_SIZE  256U

/*
 * Idle gap that ends a receive frame.
 * Timer_A0: SMCLK/8 = 1 MHz → 50000 ticks ≈ 50 ms.
 */
#define RX_IDLE_TIMER_TICKS  50000U

static volatile uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint16_t rx_len;              /* next write index / byte count */

/* Placeholder for a completed UART frame (after ~50 ms RX idle). */
static void rx_frame_placeholder(volatile uint8_t *buf, uint16_t len)
{
    /* TODO: process the received frame here */
    if (len > 0U) {
        (void)buf[0];                         /* keep buffer live for tooling/builds */
    }
}

static void clock_init_8mhz(void)
{
    /* One FRAM wait state required near 8 MHz MCLK on this family */
    FRCTL0 = FRCTLPW | NWAITS_1;

    CSCTL0_H = CSKEY_H;                       /* unlock CS */
    CSCTL1 = DCOFSEL_3 | DCORSEL;             /* DCO = 8 MHz */
    CSCTL2 = SELA__VLOCLK | SELS__DCOCLK | SELM__DCOCLK;
    CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;     /* ACLK/SMCLK/MCLK /1 */
    CSCTL0_H = 0;                             /* lock CS */
}

static void rx_idle_timer_init(void)
{
    /* Stopped until the first RX byte; CCR0 interrupt marks end of frame */
    TA0CTL = TASSEL__SMCLK | ID__8 | MC__STOP | TACLR;
    TA0EX0 = TAIDEX_0;                        /* extra /1 → SMCLK/8 = 1 MHz */
    TA0CCR0 = RX_IDLE_TIMER_TICKS - 1U;       /* ~50 ms */
    TA0CCTL0 = CCIE;
}

static void rx_idle_timer_restart(void)
{
    /* Clear count and (re)start up-mode so each byte resets the ~50 ms gap */
    TA0CTL = TASSEL__SMCLK | ID__8 | MC__UP | TACLR;
}

static void rx_idle_timer_stop(void)
{
    TA0CTL = TASSEL__SMCLK | ID__8 | MC__STOP;
}

static void uart_init_uca0(void)
{
    /* P2.0 = UCA0TXD, P2.1 = UCA0RXD */
    P2SEL1 |= (BIT0 | BIT1);
    P2SEL0 &= ~(BIT0 | BIT1);

    /* Hold eUSCI_A0 in reset while configuring */
    UCA0CTLW0 = UCSWRST;
    UCA0CTLW0 |= UCSSEL__SMCLK;               /* BRCLK = SMCLK */
    /* 8N1: UC7BIT=0, UCSPB=0, UCPEN=0 (power-up / reset defaults) */
    UCA0BRW = UART_UCBRW;
    UCA0MCTLW = UART_UCMCTLW;
    UCA0CTLW0 &= ~UCSWRST;                    /* release for operation */

    UCA0IE |= UCRXIE;                         /* enable RX interrupt */
}

static void uart_putc(char c)
{
    while (!(UCA0IFG & UCTXIFG)) {
        /* wait for TX buffer empty */
    }
    UCA0TXBUF = (unsigned char)c;
}

static void uart_puts(const char *s)
{
    while (*s) {
        uart_putc(*s++);
    }
}

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;

    clock_init_8mhz();
    rx_idle_timer_init();

    P1DIR |= BIT0;                            /* LED / demo GPIO */
    P1OUT &= ~BIT0;

    uart_init_uca0();

    PM5CTL0 &= ~LOCKLPM5;                     /* unlock GPIO */

    __enable_interrupt();

    for (;;) {
        volatile unsigned long i;

        P1OUT ^= BIT0;
        uart_puts("Hello World\r\n");

        /* Rough ~0.5 s delay at 8 MHz; RX + idle-timer ISRs still run */
        i = 400000UL;
        do {
            i--;
        } while (i != 0);
    }
}

/*
 * eUSCI_A0 RX ISR — append bytes from index 0; restart idle timer.
 */
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = EUSCI_A0_VECTOR
__interrupt void eusci_a0_isr(void)
#elif defined(__GNUC__)
void __attribute__((interrupt(EUSCI_A0_VECTOR))) eusci_a0_isr(void)
#else
#error "Unsupported compiler for EUSCI_A0 ISR"
#endif
{
    switch (UCA0IV) {
    case UCIV__NONE:
        break;
    case UCIV__UCRXIFG: {
        uint8_t c = (uint8_t)UCA0RXBUF;       /* reading RXBUF clears UCRXIFG */

        if (rx_len < RX_BUF_SIZE) {
            rx_buf[rx_len] = c;
            rx_len++;
            rx_idle_timer_restart();
        }
        /* If full, wait for idle timeout to hand off and reset */
        break;
    }
    default:
        break;
    }
}

/*
 * Timer_A0 CCR0 — ~50 ms since last RX byte → frame complete.
 */
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = TIMER0_A0_VECTOR
__interrupt void timer0_a0_isr(void)
#elif defined(__GNUC__)
void __attribute__((interrupt(TIMER0_A0_VECTOR))) timer0_a0_isr(void)
#else
#error "Unsupported compiler for TIMER0_A0 ISR"
#endif
{
    uint16_t len;

    rx_idle_timer_stop();                     /* one-shot until next RX byte */

    len = rx_len;
    if (len > 0U) {
        rx_frame_placeholder(rx_buf, len);    /* hand off buffer pointer */
        rx_len = 0U;                          /* next frame starts at index 0 */
    }
}
