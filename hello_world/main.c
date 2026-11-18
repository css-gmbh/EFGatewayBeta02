/*
 * MSP430FR5962 hello world — P1.0 blink + eUSCI_A0 UART
 *
 * UART (UCA0): TX = P2.0, RX = P2.1, 19200 baud, 8N1
 * Clock: DCO / SMCLK = 8 MHz (TI recommended baud settings for 19200)
 *
 * RX is interrupt-driven (UCRXIE + EUSCI_A0_VECTOR) into a ring buffer.
 * Main loop drains the buffer (echo), blinks P1.0, and periodically TX's
 * "Hello World\r\n".
 */

#include <msp430.h>
#include <stdint.h>

/* TI UG recommended for 8 MHz SMCLK, 19200 baud, oversampling (UCOS16=1):
 * UCBRx=26, UCBRFx=0, UCBRSx=0xB6
 */
#define UART_UCBRW   26U
#define UART_UCMCTLW (UCOS16 | (0xB6U << 8))

#define RX_BUF_SIZE  64U

static volatile uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint8_t rx_head; /* next write index (ISR) */
static volatile uint8_t rx_tail; /* next read index (main) */
static volatile uint8_t rx_overflow;

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

/* Non-blocking: 1 if a byte was read into *out, else 0 */
static int uart_rx_try_get(uint8_t *out)
{
    uint8_t head;
    uint8_t tail;

    __disable_interrupt();
    head = rx_head;
    tail = rx_tail;
    if (head == tail) {
        __enable_interrupt();
        return 0;
    }
    *out = rx_buf[tail];
    rx_tail = (uint8_t)((tail + 1U) % RX_BUF_SIZE);
    __enable_interrupt();
    return 1;
}

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;

    clock_init_8mhz();

    P1DIR |= BIT0;                            /* LED / demo GPIO */
    P1OUT &= ~BIT0;

    uart_init_uca0();

    PM5CTL0 &= ~LOCKLPM5;                     /* unlock GPIO */

    __enable_interrupt();

    for (;;) {
        volatile unsigned long i;
        uint8_t c;

        /* Drain interrupt RX ring; echo each received byte */
        while (uart_rx_try_get(&c)) {
            uart_putc((char)c);
        }

        P1OUT ^= BIT0;
        uart_puts("Hello World\r\n");

        /* Rough ~0.5 s delay at 8 MHz; RX ISR still runs */
        i = 400000UL;
        do {
            i--;
            while (uart_rx_try_get(&c)) {
                uart_putc((char)c);
            }
        } while (i != 0);

        (void)rx_overflow;
    }
}

/*
 * eUSCI_A0 RX ISR — store bytes in the ring buffer.
 * Works with TI CCS (#pragma vector) and msp430-elf-gcc (interrupt attribute).
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
        uint8_t next = (uint8_t)((rx_head + 1U) % RX_BUF_SIZE);

        if (next != rx_tail) {
            rx_buf[rx_head] = c;
            rx_head = next;
        } else {
            rx_overflow = 1;                  /* drop byte if ring is full */
        }
        break;
    }
    default:
        break;
    }
}
