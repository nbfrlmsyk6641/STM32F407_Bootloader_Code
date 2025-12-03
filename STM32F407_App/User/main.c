#include "main.h"

int main(void)
{
	// 设置向量表偏移地址
	SCB->VTOR = APP_ADDRESS;

	// 在bootloader跳转到APP之前，执行了关闭所有中断的操作，在这里需要重新打开
	__enable_irq();

    // 初始化各个模块
	TIM3_Configuration();
	LED_GPIO_Config();	
	CAN_1_Init();
	FLASH_Store_Init();
	BKP_Init();
    ISOTP_Init();
    UDS_Init();

	// 初始化关闭LED1
	LED1_OFF;

	// 初始化清空BKP
	BKP_WriteRegister(BKP_FLAG_REGISTER, 0x00000000);

	while(1)
	{
        // LED闪烁任务
        LED_Task();

        // ISO15765传输层
        ISO15765_CAN();

        // UDS应用层
        UDS_Process();

		// ISO15765超时机制
		ISOTP_Timer_NCr();
    }
	
}



