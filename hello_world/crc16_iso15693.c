/*
 * ISO/IEC 15693-style CRC-16 with generator polynomial 0x0810.
 */

#include "crc16_iso15693.h"

uint16_t crc16_iso15693_update(uint16_t crc, const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint8_t bit;

    if ((data == 0) && (len > 0U)) {
        return crc;
    }

    for (i = 0U; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (bit = 0U; bit < 8U; bit++) {
            if ((crc & 0x0001U) != 0U) {
                crc = (uint16_t)((crc >> 1) ^ CRC16_ISO15693_POLY);
            } else {
                crc = (uint16_t)(crc >> 1);
            }
        }
    }

    return crc;
}

uint16_t crc16_iso15693_finalize(uint16_t crc)
{
    return (uint16_t)(crc ^ CRC16_ISO15693_XOROT);
}

uint16_t crc16_iso15693(const uint8_t *data, uint16_t len)
{
    return crc16_iso15693_finalize(
        crc16_iso15693_update(CRC16_ISO15693_INIT, data, len));
}
