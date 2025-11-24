#ifndef _CRC_H_
#define _CRC_H_

uint32_t IAP_Software_Calculate_CRC(const uint8_t * pData, uint32_t uiDataSize);
uint32_t IAP_Calculate_CRC_On_Flash(uint32_t start_address, uint32_t size_in_bytes);

#endif /* _CRC_H_ */


