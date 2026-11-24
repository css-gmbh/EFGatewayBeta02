/*
 * eUSCI_A2 UART (same framing/baud as UCA0)
 *
 *   TX = P5.4 (UCA2TXD)
 *   RX = P5.5 (UCA2RXD)
 *   19200 baud, 8N1, SMCLK = 8 MHz
 */

#ifndef UART_UCA2_H
#define UART_UCA2_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UCA2_RX_BUF_SIZE  256U
#define UCA2_TX_BUF_SIZE  256U

/** Configure pins, UCA2, RX IRQ + idle timer. */
void uart_uca2_init(void);

/** Non-blocking interrupt TX. Returns 1 if accepted, 0 if busy/invalid. */
uint8_t uart_uca2_send(const uint8_t *data, uint16_t len);

/** Convenience: send a NUL-terminated string. */
uint8_t uart_uca2_send_str(const char *s);

/** 1 while an interrupt TX is in progress. */
uint8_t uart_uca2_tx_busy(void);

/**
 * Called from Timer_A2 idle ISR after ~50 ms RX silence.
 * Default stub CRC-touches the buffer; override in the app if needed.
 */
void uart_uca2_rx_frame_placeholder(volatile uint8_t *buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* UART_UCA2_H */
