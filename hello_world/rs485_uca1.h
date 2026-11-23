/*
 * eUSCI_A1 UART as half-duplex RS-485
 *
 *   TX  = P2.5 (UCA1TXD)
 *   RX  = P2.6 (UCA1RXD)
 *   DE  = P4.3 (driver enable, board-inverted: active low)
 *   /RE = P4.2 (receiver enable, board-inverted: active high)
 *   115200 baud, 8N1, SMCLK = 8 MHz
 */

#ifndef RS485_UCA1_H
#define RS485_UCA1_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RS485_RX_BUF_SIZE  256U
#define RS485_TX_BUF_SIZE  256U

/** Configure pins, UCA1, DE/RE (idle = receive), RX IRQ + idle timer. */
void rs485_init(void);

/** Non-blocking interrupt TX. Returns 1 if accepted, 0 if busy/invalid. */
uint8_t rs485_send(const uint8_t *data, uint16_t len);

/** 1 while an interrupt TX is in progress (including last-bit complete). */
uint8_t rs485_tx_busy(void);

/**
 * Optional app hook: called from Timer_A1 idle ISR after ~50 ms RX silence.
 * Default weak stub does nothing; override in application if needed.
 */
void rs485_rx_frame_placeholder(volatile uint8_t *buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* RS485_UCA1_H */
