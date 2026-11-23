/*
 * Half-duplex RS-485 on P2.5/P2.6 with inverted UART polarity.
 *
 * MSP430FR5962 eUSCI_A1 has no UCTXINV/UCRXINV, so TX/RX use a Timer_A2
 * soft-UART (idle-low / start-high) on the same pins. DE/RE stay classic
 * MAX485 polarity (DE active high, /RE active low).
 */

#include <msp430.h>
#include <stdint.h>

#include "crc16_iso15693.h"
#include "rs485_uca1.h"

#define RS485_IDLE_TIMER_TICKS  50000U        /* ~50 ms at SMCLK/8 = 1 MHz */

#define RS485_DE_BIT   BIT3                   /* P4.3 */
#define RS485_RE_BIT   BIT2                   /* P4.2 */
#define RS485_TX_BIT   BIT5                   /* P2.5 */
#define RS485_RX_BIT   BIT6                   /* P2.6 */

/* 8 MHz / 57600 ≈ 138.89 → 139 ticks (~-0.08 % baud error) */
#define RS485_BIT_TICKS       139U
#define RS485_HALF_BIT_TICKS  (RS485_BIT_TICKS / 2U)

#define SOFT_IDLE  0U
#define SOFT_TX    1U
#define SOFT_RX    2U

static volatile uint8_t rs485_rx_buf[RS485_RX_BUF_SIZE];
static volatile uint16_t rs485_rx_len;

static uint8_t rs485_tx_buf[RS485_TX_BUF_SIZE];
static volatile uint16_t rs485_tx_len;
static volatile uint16_t rs485_tx_index;
static volatile uint8_t rs485_tx_active;

static volatile uint8_t soft_state;
static volatile uint16_t soft_shift;          /* LSB = next wire bit */
static volatile uint8_t soft_bits_left;

/* DE high = drive bus; /RE low = receive enabled (classic MAX485) */
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

static void rs485_rx_irq_enable(void)
{
    P2IFG &= (uint8_t)~RS485_RX_BIT;
    P2IE |= RS485_RX_BIT;
}

static void rs485_rx_irq_disable(void)
{
    P2IE &= (uint8_t)~RS485_RX_BIT;
    P2IFG &= (uint8_t)~RS485_RX_BIT;
}

/* Inverted UART 10-bit frame: start=1, data=~byte (LSB first), stop=0 */
static uint16_t rs485_make_inv_frame(uint8_t byte)
{
    uint16_t normal = ((uint16_t)byte << 1) | 0x200U; /* start0 + data + stop1 */
    return (uint16_t)((~normal) & 0x3FFU);
}

static void rs485_bit_timer_stop(void)
{
    TA2CCTL0 = 0;
    TA2CTL = TASSEL__SMCLK | MC__STOP | TACLR;
}

static void rs485_bit_timer_start(uint16_t first_delay)
{
    TA2CTL = TASSEL__SMCLK | MC__STOP | TACLR;
    TA2CCR0 = first_delay;
    TA2CCTL0 = CCIE;
    TA2CTL = TASSEL__SMCLK | MC__UP | TACLR;
}

static void rs485_tx_line(uint8_t high)
{
    if (high != 0U) {
        P2OUT |= RS485_TX_BIT;
    } else {
        P2OUT &= (uint8_t)~RS485_TX_BIT;
    }
}

static void rs485_tx_start_byte(void)
{
    soft_shift = rs485_make_inv_frame(rs485_tx_buf[rs485_tx_index]);
    rs485_tx_index++;
    soft_bits_left = 10U;
    soft_state = SOFT_TX;

    /* Drive start bit now; timer advances remaining bits */
    rs485_tx_line((uint8_t)(soft_shift & 1U));
    soft_shift >>= 1;
    soft_bits_left--;
    rs485_bit_timer_start(RS485_BIT_TICKS);
}

void rs485_rx_frame_placeholder(volatile uint8_t *buf, uint16_t len)
{
    uint16_t crc = crc16_iso15693((const uint8_t *)buf, len);
    (void)crc;
    /* TODO: process RS-485 frame */
}

void rs485_init(void)
{
    /* DE / /RE as GPIO outputs; start in receive (classic MAX485 levels) */
    P4DIR |= (RS485_DE_BIT | RS485_RE_BIT);
    rs485_set_rx_mode();

    /*
     * Soft-UART GPIO on P2.5 (TX) / P2.6 (RX) — not eUSCI (no line invert).
     * Inverted polarity: idle low, start high.
     */
    P2SEL1 &= (uint8_t)~(RS485_TX_BIT | RS485_RX_BIT);
    P2SEL0 &= (uint8_t)~(RS485_TX_BIT | RS485_RX_BIT);
    P2DIR |= RS485_TX_BIT;
    P2DIR &= (uint8_t)~RS485_RX_BIT;
    rs485_tx_line(0U);                        /* idle = low */

    /* RX start = rising edge (inverted start bit) */
    P2IES &= (uint8_t)~RS485_RX_BIT;
    rs485_rx_irq_enable();

    /* Leave eUSCI_A1 in reset — pins are GPIO soft-UART */
    UCA1CTLW0 = UCSWRST;

    /* Frame idle timer (Timer_A1 CCR0) — stopped until first RX byte */
    TA1CTL = TASSEL__SMCLK | ID__8 | MC__STOP | TACLR;
    TA1EX0 = TAIDEX_0;
    TA1CCR0 = RS485_IDLE_TIMER_TICKS - 1U;
    TA1CCTL0 = CCIE;

    rs485_bit_timer_stop();

    rs485_rx_len = 0U;
    rs485_tx_active = 0U;
    soft_state = SOFT_IDLE;
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

    rs485_rx_irq_disable();
    rs485_set_tx_mode();
    rs485_tx_start_byte();
    __enable_interrupt();
    return 1U;
}

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = PORT2_VECTOR
__interrupt void port2_isr(void)
#elif defined(__GNUC__)
void __attribute__((interrupt(PORT2_VECTOR))) port2_isr(void)
#else
#error "Unsupported compiler for PORT2 ISR"
#endif
{
    switch (P2IV) {
    case P2IV__NONE:
        break;

    case P2IV__P2IFG6:                        /* P2.6 rising = inverted start */
        if (rs485_tx_active != 0U) {
            break;
        }
        rs485_rx_irq_disable();
        soft_state = SOFT_RX;
        soft_shift = 0U;
        soft_bits_left = 8U;                  /* data bits after centering */
        /* Mid-first-data-bit = 1.5 bit times from start edge */
        rs485_bit_timer_start(RS485_BIT_TICKS + RS485_HALF_BIT_TICKS);
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
    if (soft_state == SOFT_TX) {
        if (soft_bits_left > 0U) {
            rs485_tx_line((uint8_t)(soft_shift & 1U));
            soft_shift >>= 1;
            soft_bits_left--;
            TA2CCR0 = RS485_BIT_TICKS;
        } else if (rs485_tx_index < rs485_tx_len) {
            rs485_tx_start_byte();
        } else {
            /* Stop bit already held one bit-time; release bus */
            rs485_bit_timer_stop();
            rs485_tx_line(0U);                /* idle low */
            rs485_set_rx_mode();
            rs485_tx_active = 0U;
            soft_state = SOFT_IDLE;
            rs485_rx_irq_enable();
        }
    } else if (soft_state == SOFT_RX) {
        if (soft_bits_left > 0U) {
            /* Wire is inverted: high means logical 0 data bit */
            if ((P2IN & RS485_RX_BIT) == 0U) {
                soft_shift |= (uint16_t)(1U << (8U - soft_bits_left));
            }
            soft_bits_left--;
            TA2CCR0 = RS485_BIT_TICKS;
        } else {
            /* Optional stop-bit sample ignored; accept byte */
            rs485_bit_timer_stop();
            soft_state = SOFT_IDLE;
            if (rs485_rx_len < RS485_RX_BUF_SIZE) {
                rs485_rx_buf[rs485_rx_len] = (uint8_t)soft_shift;
                rs485_rx_len++;
                rs485_idle_timer_restart();
            }
            rs485_rx_irq_enable();
        }
    } else {
        rs485_bit_timer_stop();
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
