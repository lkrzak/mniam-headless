#include "amcom.h"

#include <stdint.h>
#include <string.h>

/// Start of packet character
const uint8_t  AMCOM_SOP         = 0xA1;
const uint16_t AMCOM_INITIAL_CRC = 0xFFFF;

/**
 * Updates the CRC given a byte of data
 *
 * @param byte byte of data
 * @param crc current crc
 * @return new crc value
 */
static uint16_t AMCOM_UpdateCRC(uint8_t byte, uint16_t crc) {
    byte ^= (uint8_t)(crc & 0x00ff);
    byte ^= (uint8_t)(byte << 4);
    return ((((uint16_t)byte << 8) | (uint8_t)(crc >> 8)) ^ (uint8_t)(byte >> 4) ^ ((uint16_t)byte << 3));
} /* CRC16_UpdateCRC_Byte */


void AMCOM_InitReceiver(AMCOM_Receiver* receiver, AMCOM_PacketHandler packetHandlerCallback, void* userContext) {
    // TODO
}

size_t AMCOM_Serialize(uint8_t packetType, const void* payload, size_t payloadSize, uint8_t* destinationBuffer) {
    // TODO
    return 0;
}

void AMCOM_Deserialize(AMCOM_Receiver* receiver, const void* data, size_t dataSize) {
    // TODO
}
