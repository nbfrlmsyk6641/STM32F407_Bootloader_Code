#ifndef _FLASH_H_
#define _FLASH_H_

uint32_t FLASH_ReadWord(uint32_t Address);
uint16_t FLASH_ReadHalfWord(uint32_t Address);
uint8_t FLASH_ReadByte(uint32_t Address);
void FLASH_EraseAllPage(void);
void FLASH_ErasePage(uint32_t Sector);
void FLASH_WriteWord(uint32_t Address, uint32_t Data);
void FLASH_WriteHalfWord(uint32_t Address, uint16_t Data);

#endif
