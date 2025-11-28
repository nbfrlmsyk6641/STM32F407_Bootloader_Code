#include "main.h"

// 定义APP的起始地址，起始地址必须为4字节对齐
#define APP_ADDRESS    (uint32_t)0x08008000

// 诊断设备的物理请求ID
#define UDS_PHYSICAL_REQUEST_ID   0x7E0

// UDS协议会话定义
#define UDS_SESSION_DEFAULT       0x01
#define UDS_SESSION_PROGRAMMING   0x02
#define UDS_SESSION_EXTENDED      0x03

// UDS协议会话状态变量
uint8_t g_current_session;

// UDS预编程会话满足条件标志
uint8_t g_pre_prog_condition = 0;

// UDS服务时间计数
uint32_t g_s3_server_timer = 0;

// UDS超时
#define S3_TIMEOUT_VALUE  5000

// ISO15765协议上下文变量
extern IsoTpLink_t g_isotp;

// ISO15765协议变量
uint8_t sid;
uint8_t sub_func;
uint8_t ISO_resp_data[2];
uint32_t current_id;

// 定义周期性发送报文变量
uint32_t Cyc_TxID = 0x001;
uint8_t Cyc_TxLength = 8;
uint8_t Cyc_TxData[8] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11};

// 定义环形缓冲器变量
CanRxMsg roll_recv_msg;
uint8_t roll_tx_data[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint32_t roll_msg_id;

// 回应变量数组定义
uint8_t resp2[6];
uint8_t resp3[6];
uint8_t resp4[6];
uint8_t resp5[4];
uint8_t resp6[2];

// 重置服务计时器
void UDS_Reset_S3_Timer(void)
{
    g_s3_server_timer = S3_TIMEOUT_VALUE;
}

// 发送否定响应
void UDS_Send_NRC(uint8_t sid, uint8_t nrc)
{
	uint8_t resp1[3];
    resp1[0] = 0x7F; // Negative Response
    resp1[1] = sid;  // 哪个服务失败了
    resp1[2] = nrc;  // 失败原因
    ISOTP_Transmit_SF(0x7E8, resp1, 3);
}

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

    // 初始化会话为默认会话
    g_current_session = UDS_SESSION_DEFAULT;

	while(1)
	{
        // 每0.2秒翻转一次LED
        if (g_timer_1s_flag == 1)
        {
            g_timer_1s_flag = 0;
            GPIO_ToggleBits(GPIOE, GPIO_Pin_13);
        }

        // ISO-TP传输层
        if(CAN_RingBuffer_Read(&roll_recv_msg) == 1)
        {
            // 收到任何有效物理寻址报文，重置服务定时器
            UDS_Reset_S3_Timer();

            if (roll_recv_msg.IDE == CAN_Id_Standard)
            {
                current_id = roll_recv_msg.StdId;
            }
            else
            {
                current_id = roll_recv_msg.ExtId;
            }
            
            if (current_id == UDS_PHYSICAL_REQUEST_ID)
            {
                // UDS服务，调用ISO-TP处理
                ISOTP_Receive_Handler(&roll_recv_msg);
            }

        }

        // UDS应用层
        if (g_isotp.state == ISOTP_RX_COMPLETE)
        {
            uint8_t sid = g_isotp.rx_buffer[0];
            uint8_t sub_func = 0;
            uint8_t* p_data = &g_isotp.rx_buffer[1];

            if (g_isotp.rx_total_len > 1)
            {
                // 取出子功能
                sub_func = g_isotp.rx_buffer[1] & 0x7F;
            }

            // 处理UDS服务
            switch (sid)
            {   
                // 处理10服务
                case 0x10:
                {
                    if (sub_func == UDS_SESSION_DEFAULT)
                    {
                        // 切换回默认会话， 10 01
                        g_current_session = UDS_SESSION_DEFAULT;
                        g_pre_prog_condition = 0; // 清除权限
                        
                        // 肯定响应: 50 01
						resp2[0] = 0x50;
                        resp2[1] = 0x01;
                        resp2[2] = 0x00;
                        resp2[3] = 0x32;
                        resp2[4] = 0x01;
                        resp2[5] = 0xF4;
                        ISOTP_Transmit_SF(0x7E8, resp2, 6);
                    }
                    else if (sub_func == UDS_SESSION_EXTENDED)
                    {
                        // 切换到扩展会话， 10 03
                        g_current_session = UDS_SESSION_EXTENDED;
                        
                        // 肯定响应: 50 03
                        resp3[0] = 0x50;
                        resp3[1] = 0x03;
                        resp3[2] = 0x00;
                        resp3[3] = 0x32;
                        resp3[4] = 0x01;
                        resp3[5] = 0xF4;
                        ISOTP_Transmit_SF(0x7E8, resp3, 6);
                    }
                    else if (sub_func == UDS_SESSION_PROGRAMMING)
                    {
                        // 条件检查，满足条件才进入编程会话，10 02
                        if (g_current_session == UDS_SESSION_EXTENDED && g_pre_prog_condition == 1)
                        {
                            // 肯定响应: 50 02
                            resp4[0] = 0x50;
                            resp4[1] = 0x02;
                            resp4[2] = 0x00;
                            resp4[3] = 0x32;
                            resp4[4] = 0x01;
                            resp4[5] = 0xF4;
                            ISOTP_Transmit_SF(0x7E8, resp4, 6);

                            // 写BKP标志，系统复位进Bootloader
                            BKP_WriteRegister(BKP_FLAG_REGISTER, 0xAaBbCcDd);
                            NVIC_SystemReset();
                        }
                        else
                        {
                            UDS_Send_NRC(0x10, 0x22); 
                        }
                    }
                    else
                    {
                        // 不支持的会话类型 (NRC 0x12: Sub-function not supported)
                        UDS_Send_NRC(0x10, 0x12);
                    }
                }
                break;

                // 处理31服务
                case 0x31:
                {
                    // 检查会话状态
                    if (g_current_session != UDS_SESSION_EXTENDED)
                    {
                        // 服务在当前会话不支持
                        UDS_Send_NRC(0x31, 0x7F);
                        break;
                    }

                    // 解析 Routine ID
                    // p_data[0] = Type(01), p_data[1]=ID_High, p_data[2]=ID_Low
                    if (p_data[0] == 0x01 && p_data[1] == 0xFF && p_data[2] == 0x00)
                    {
                        // 执行检查逻辑 (模拟：车速=0，电压>12V)
                        g_pre_prog_condition = 1;

                        // 肯定响应: 71 01 FF 00
                        resp5[0] = 0x71;
                        resp5[1] = 0x01;
                        resp5[2] = 0xFF;
                        resp5[3] = 0x00;
                        
                        ISOTP_Transmit_SF(0x7E8, resp5, 4);
                    }
                    else
                    {
                        // 请求超出范围 (ID不对)
                        UDS_Send_NRC(0x31, 0x31);
                    }
                }
                break;

                case 0x3E:
                {
                    // 肯定响应: 7E 00
                    resp6[0] = 0x7E;
                    resp6[1] = 0x00;
                    
                    ISOTP_Transmit_SF(0x7E8, resp6, 2);
                }
                break;

                default:
                    // 服务不支持
                    UDS_Send_NRC(sid, 0x11);
                    break;
            }
            // 重置ISO-TP状态，准备接收下一个报文
            ISOTP_Init();
        }
        else if (g_isotp.state == ISOTP_RX_ERROR)
        {
            ISOTP_Init();
        }
    }
	
}



