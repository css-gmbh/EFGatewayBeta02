/*
 * MSP430FR5962 hello world — P1.0 blink + eUSCI_A0 UART
 *
 * UART (UCA0): TX = P2.0, RX = P2.1, 19200 baud, 8N1
 * Clock: DCO / SMCLK = 8 MHz (TI recommended baud settings for 19200)
 *
 * Periodically toggles P1.0 and transmits "Hello World\r\n" on UCA0TXD.
 */

#include <msp430.h>

/* TI UG recommended for 8 MHz SMCLK, 19200 baud, oversampling (UCOS16=1):
 * UCBRx=26, UCBRFx=0, UCBRSx=0xB6
 */
#define UART_UCBRW   26U
#define UART_UCMCTLW (UCOS16 | (0xB6U << 8))

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

    P1DIR |= BIT0;                            /* LED / demo GPIO */
    P1OUT &= ~BIT0;

    uart_init_uca0();

    PM5CTL0 &= ~LOCKLPM5;                     /* unlock GPIO */

    for (;;) {
        volatile unsigned long i;

        P1OUT ^= BIT0;
        uart_puts("Hello World\r\n");

        /* Rough ~0.5 s delay at 8 MHz */
        i = 400000UL;
        do {
            i--;
        } while (i != 0);
    }
}
