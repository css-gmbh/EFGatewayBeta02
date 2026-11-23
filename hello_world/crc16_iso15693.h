/*
 * ISO/IEC 15693-style CRC-16 helper
 *
 * Algorithm framing matches ISO 15693 / ISO 13239 CRC-16 usage:
 *   - width 16
 *   - initial value 0xFFFF
 *   - reflected (LSB-first) bit processing
 *   - final XOR 0xFFFF
 *
 * Generator polynomial for this project: 0x0810
 * (Standard ISO 15693 text uses x^16+x^12+x^5+1 = 0x1021 / reflected 0x8408;
 *  this unit intentionally uses the requested 0x0810.)
 */

#ifndef CRC16_ISO15693_H
#define CRC16_ISO15693_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Generator polynomial (LSB-first / reflected form as used by the bit engine). */
#define CRC16_ISO15693_POLY  0x0810U

/** ISO 15693-style preset / final XOR value. */
#define CRC16_ISO15693_INIT  0xFFFFU
#define CRC16_ISO15693_XOROT 0xFFFFU

/**
 * Compute CRC-16 over a byte buffer.
 *
 * @param data  Pointer to bytes (may be NULL if len == 0)
 * @param len   Number of bytes
 * @return      CRC-16 after final XOR (ready to append / compare)
 */
uint16_t crc16_iso15693(const uint8_t *data, uint16_t len);

/**
 * Running update (seed with CRC16_ISO15693_INIT; finish with
 * crc16_iso15693_finalize()).
 */
uint16_t crc16_iso15693_update(uint16_t crc, const uint8_t *data, uint16_t len);

/** Apply final XOR (ISO 15693-style). */
uint16_t crc16_iso15693_finalize(uint16_t crc);

#ifdef __cplusplus
}
#endif

#endif /* CRC16_ISO15693_H */
