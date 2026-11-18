/*
 * MSP430FR5962 hello world — software toggle of P1.0
 *
 * Conventional TI FRAM blink demo: stop the WDT, unlock GPIO (LOCKLPM5),
 * drive P1.0 as an output, and toggle it in a busy loop.
 *
 * P1.0 is the standard TI demo LED pin on many MSP430 LaunchPads
 * (e.g. green LED2 on MSP-EXP430FR5969). Remap if your board differs;
 * this repository does not document custom EF Gateway LED wiring yet.
 */

#include <msp430.h>

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD; /* Stop watchdog timer */

    /*
     * FRAM devices power up with I/O locked high-Z until LOCKLPM5 is cleared.
     * Configure direction first, then unlock.
     */
    P1DIR |= BIT0;            /* P1.0 output */
    P1OUT &= ~BIT0;           /* start low */
    PM5CTL0 &= ~LOCKLPM5;     /* unlock GPIO configuration */

    for (;;) {
        volatile unsigned int i;

        P1OUT ^= BIT0;        /* toggle P1.0 */

        /* Rough software delay (~depends on default DCO) */
        i = 50000;
        do {
            i--;
        } while (i != 0);
    }
}
