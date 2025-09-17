#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H

// App起始地址
#define APP_ADDRESS    (uint32_t)0x08008000

void Jump_To_Application(void);

#endif /* __BOOTLOADER_H */


