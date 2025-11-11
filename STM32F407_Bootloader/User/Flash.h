#ifndef _FLASH_H_
#define _FLASH_H_

 // 测试存储区起始地址及大小定义
#define STORE_START_ADDRESS ((uint32_t)0x080E0000)
#define STORE_COUNT		     512
#define FLASH_SECTOR_11      FLASH_Sector_11


// Application起始地址
#define APPLICATION_START_ADDRESS ((uint32_t)0x08008000)

// 分配给Application的空间所占用的Flash扇区
#define FLASH_SECTOR_2      FLASH_Sector_2
#define FLASH_SECTOR_3      FLASH_Sector_3
#define FLASH_SECTOR_4      FLASH_Sector_4
#define FLASH_SECTOR_5      FLASH_Sector_5
#define FLASH_SECTOR_6      FLASH_Sector_6

// 分配给Application的空间所占用的扇区的起始地址
#define APPLICATION_SECTOR_2   ((uint32_t)0x08008000)
#define APPLICATION_SECTOR_3   ((uint32_t)0x0800C000)
#define APPLICATION_SECTOR_4   ((uint32_t)0x08010000)
#define APPLICATION_SECTOR_5   ((uint32_t)0x08020000)
#define APPLICATION_SECTOR_6   ((uint32_t)0x08040000)

// 分配给Application的扇区数量
#define APP_SECTOR_COUNT 5

uint32_t FLASH_ReadWord(uint32_t Address);
uint16_t FLASH_ReadHalfWord(uint32_t Address);
uint8_t FLASH_ReadByte(uint32_t Address);
void MyFLASH_EraseAllSectors(void);
void MyFLASH_EraseSectors(uint32_t Sector);
void FLASH_WriteWord(uint32_t Address, uint32_t Data);
void FLASH_WriteHalfWord(uint32_t Address, uint16_t Data);
FLASH_Status FLASH_Write_Buffer(uint32_t Address, uint16_t* pData, uint16_t Count);
void FLASH_Store_Init(void);
void FLASH_Store_Save(void);
void FLASH_Store_Clear(void);
uint8_t IAP_Erase_App_Sectors(uint32_t firmware_size);

#endif
