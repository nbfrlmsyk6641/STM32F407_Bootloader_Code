#include "main.h"

// 定义APP的起始地址，起始地址必须为4字节对齐
#define APP_ADDRESS    (uint32_t)0x08008000

// 诊断设备的物理请求ID
#define UDS_PHYSICAL_REQUEST_ID   0x7E0

// ISO15765协议上下文变量
extern IsoTpLink_t g_isotp;

// ISO15765协议变量
uint8_t sid;
uint8_t sub_func;
uint8_t ISO_resp_data[2];

// 定义周期性发送报文变量
uint32_t Cyc_TxID = 0x001;
uint8_t Cyc_TxLength = 8;
uint8_t Cyc_TxData[8] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11};

// 定义环形缓冲器变量
CanRxMsg roll_recv_msg;
uint8_t roll_tx_data[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint32_t roll_msg_id;

int main(void)
{
	// 设置向量表偏移地址
	SCB->VTOR = APP_ADDRESS;

	// 在bootloader跳转到APP之前，执行了关闭所有中断的操作，在这里需要重新打开
	__enable_irq();

	TIM3_Configuration();
	LED_GPIO_Config();	
	CAN_1_Init();
	FLASH_Store_Init();
	BKP_Init();
    ISOTP_Init();

	// 初始化关闭LED1
	LED1_OFF;

	// 初始化清空BKP
	BKP_WriteRegister(BKP_FLAG_REGISTER, 0x00000000);

	while(1)
	{
		// 从环形缓冲器中读取报文进行处理
		if (CAN_RingBuffer_Read(&roll_recv_msg) == 1)
        {
            if (roll_recv_msg.IDE == CAN_Id_Standard)
            {
                roll_msg_id = roll_recv_msg.StdId;
            }
            else
            {
                roll_msg_id = roll_recv_msg.ExtId;
            }

            // 检查是否是重启指令 (ID: 0xC0, DLC: 8, Data: 11 11 11)
            if( (roll_msg_id == 0xC0) && 
                (roll_recv_msg.DLC == 8) && 
                (roll_recv_msg.Data[0] == 0x11) && 
                (roll_recv_msg.Data[1] == 0x11) &&
                (roll_recv_msg.Data[2] == 0x11))
            {
                // 1. 收到升级命令，发送确认报文 (0xA0)
                CAN1_Transmit_TX(0xA0, 8, roll_tx_data);

                // 2. 写入数据至BKP寄存器，表示收到升级命令
                BKP_WriteRegister(BKP_FLAG_REGISTER, 0xAaBbCcDd); 

                // 3. 触发系统复位，进入bootloader
                NVIC_SystemReset();
            }

            // 处理ISO15765协议接收(测试)
            if (roll_msg_id == UDS_PHYSICAL_REQUEST_ID)
            {
                ISOTP_Receive_Handler(&roll_recv_msg);
            }

            if (g_isotp.state == ISOTP_RX_COMPLETE)
            {
                sid = g_isotp.rx_buffer[0];
                sub_func = 0;

                // 判断是否有子功能
                if (g_isotp.rx_total_len > 1) 
                {
                    sub_func = g_isotp.rx_buffer[1];
                }

                if (sid == 0x11)
                {
                    ISO_resp_data[0] = 0x51;
                    ISO_resp_data[1] = sub_func;

                    // 协议发送单帧响应
                    ISOTP_Transmit_SF(0x7E8, ISO_resp_data, 2);

                    ISOTP_Init();
                }
            }
            else if (g_isotp.state == ISOTP_RX_ERROR)
            {
                // 发生错误，重置状态
                ISOTP_Init();
            }
        }

		// 每2秒执行一次的任务
		if (g_timer_1s_flag == 1)
		{
			g_timer_1s_flag = 0;

			// 翻转LED1，周期性发送CAN报文
			GPIO_ToggleBits(GPIOE, GPIO_Pin_13);
            CAN1_Transmit_TX(Cyc_TxID, Cyc_TxLength, Cyc_TxData);
		}
	}
	
}



