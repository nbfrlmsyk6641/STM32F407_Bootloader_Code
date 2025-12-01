#include "main.h" 

// UDS诊断设备发送至MCU端的CAN ID
#define UDS_PHYSICAL_REQUEST_ID   0x7E0

#define APPLICATION_START_ADDRESS ((uint32_t)0x08008000)

// ISO-TP接收上下文对象
extern IsoTpLink_t g_isotp;

// 环形缓冲器接收变量
CanRxMsg roll_recv_msg;

// UDS服务应答数组(肯定应答)
uint8_t resp1002[6] = {0x50, 0x02, 0x00, 0x32, 0x01, 0xF4};
uint8_t resp34[4] = {0x74, 0x20, 0x10, 0x00};
uint8_t pending[3] = {0x7F, 0x31, 0x78};
uint8_t resp31[4] = {0x71, 0x01, 0xFF, 0x00};
uint8_t resp36[2] = {0x76, 0xFF};
uint8_t resp37[1] = {0x77};

// UDS服务应答数组(否定应答)
uint8_t resp_31fail[3] = {0x7F, 0x31, 0x31};
uint8_t resp_36fail[3] = {0x7F, 0x36, 0x72};
uint8_t resp_37fail[3] = {0x7F, 0x37, 0x72};
uint8_t resp_fail[3] = {0x7F, 0x00, 0x11};

// 简化版状态机
typedef enum {
    STATE_IDLE,
    STATE_ERASED,
    STATE_WRITING
} Boot_State_t;

Boot_State_t boot_state = STATE_IDLE;

// IAP标志位
uint8_t IAP_Flag = 0;

// BKP变量
uint32_t BKP_Data = 0;

// 固件数据变量
uint32_t g_firmware_total_size = 0;
uint32_t g_firmware_total_crc = 0;
uint32_t g_firmware_bytes_received = 0;
uint8_t  g_expected_packet_num = 0;
uint16_t data_to_write[2050];
uint32_t write_addr = 0;
uint8_t last_ack_num = 0;
uint8_t block_seq = 0;
uint8_t* fw_data = NULL;
uint16_t fw_len = 0;
uint16_t temp_halfword;

// CRC校验变量
uint32_t local_crc = 0;

uint32_t i;
uint32_t j;

int main(void)
{
    // 初始化配置中断分组
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    // 检查BKP寄存器中的IAP请求标志
    BKP_Init();

    BKP_Data = BKP_ReadRegister(BKP_FLAG_REGISTER);

    if (BKP_Data == 0xAaBbCcDd)
    {
        // BKP清零
        BKP_WriteRegister(BKP_FLAG_REGISTER, 0x00000000);

        // 存在升级请求，准备走IAP流程
        IAP_Flag = 1;
    }
    else
    {
        // 不存在升级请求，准备执行5s初始化后进App
        IAP_Flag = 0;

        // BKP也清零
        BKP_WriteRegister(BKP_FLAG_REGISTER, 0x00000000);
    }

    if (IAP_Flag == 0)
    {
        TIM3_Configuration();
        LED_GPIO_Config();
        CAN_1_Init();

        while(1)
        {
            // 主循环代码
            if(Get_Task1_Flag() == 1)
            {
                // 5s等待过程中
                Clear_Task1_Flag();

                // 持续闪烁LED且频率高于App的LED闪烁频率
                GPIO_ToggleBits(GPIOE, GPIO_Pin_13);
            }

            if(Get_Task3_Flag() == 1)
            {
                // 5s等待过程中
                Clear_Task3_Flag();
            }

            if(Get_Task2_Flag() == 1)
            {
                // 5秒等待结束，无任何特殊请求，跳转到App
                Clear_Task2_Flag();

                // 跳转进App
                Jump_To_Application();
            }
        }
    }

    if(IAP_Flag == 1)
    {
        // 主循环代码
        TIM3_Configuration();
        LED_GPIO_Config();
        CAN_1_Init();
        ISOTP_Init();

        while(1)
        {
            // CAN报文接收缓冲
            uint32_t rx_id;
            uint8_t  rx_len;
            uint8_t  rx_data[8];

            // LED闪烁，指示正在运行IAP
            if(Get_Task1_Flag() == 1)
            {
                Clear_Task1_Flag();

                GPIO_ToggleBits(GPIOE, GPIO_Pin_13);
            }

            // 取出CAN报文,ISO-TP协议处理报文
            if(CAN_RingBuffer_Read(&roll_recv_msg) == 1)
            {
                rx_id = roll_recv_msg.StdId;

                rx_len = roll_recv_msg.DLC;

                for (i = 0; i < rx_len; i++)
                {   
                    // 取出收到的数据，这一步可有可无
                    rx_data[i] = roll_recv_msg.Data[i];
                }

                // 通过CAN ID判断是否为UDS诊断请求报文,如果是则交给协议处理
                if (rx_id == UDS_PHYSICAL_REQUEST_ID)
                {
                    ISOTP_Receive_Handler(&roll_recv_msg);
                }
            }
            else
            {   
                // 无报文接收，清一下变量，避免干扰状态机
                rx_id = 0;
                rx_len = 0;
                memset(rx_data, 0, sizeof(rx_data));
            }

            // CAN报文处理完毕，执行对应UDS服务
            if (g_isotp.state == ISOTP_RX_COMPLETE)
            {
                uint8_t sid = g_isotp.rx_buffer[0];
                uint8_t sub_func = 0;
                uint8_t* p_data = &g_isotp.rx_buffer[1];
                uint16_t data_len = g_isotp.rx_total_len - 1;

                // 取出子功能，34服务没有子功能，就不用解析
                if (g_isotp.rx_total_len > 1 && sid != 0x34)
                {
                    sub_func = g_isotp.rx_buffer[1] & 0x7F;
                }

                // 开始处理UDS服务
                switch (sid)
                {
                    // 重启进Bootloader后会再来一次10 02，确认进入Bootloader
                    case 0x10:
                    {
                        if (sub_func == 0x02)
                        {
                            // 肯定应答
                            ISOTP_Transmit_SF(0x7E8, resp1002, sizeof(resp1002));

                            // 状态机
                            boot_state = STATE_IDLE;
                        }
                    }
                    break;

                    // 34服务，下载请求，解析上位机发来的固件信息：固件大小、CRC32校验码
                    case 0x34:
                    {   
                        // 解析固件大小
                        g_firmware_total_size = (uint32_t)p_data[0] | ((uint32_t)p_data[1] << 8) | ((uint32_t)p_data[2] << 16) | ((uint32_t)p_data[3] << 24);

                        // 解析CRC32校验码
                        g_firmware_total_crc = (uint32_t)p_data[4] | ((uint32_t)p_data[5] << 8) | ((uint32_t)p_data[6] << 16) | ((uint32_t)p_data[7] << 24);

                        // 清空固件计数准备进行接收，必须清空，不然后续地址计算会错
                        g_firmware_bytes_received = 0;

                        // 应答上位机
                        ISOTP_Transmit_SF(0x7E8, resp34, sizeof(resp34));

                        // 状态机
                        boot_state = STATE_IDLE;
                    }
                    break;

                    // 31服务，Flash擦除请求，进行Flash擦除
                    case 0x31:
                    {
                        if (sub_func == 0x01)
                        {
                            if (p_data[1] == 0xFF && p_data[2] == 0x00)
                            {
                                // 回复上位机
                                ISOTP_Transmit_SF(0x7E8, pending, sizeof(pending));

                                // 根据固件大小擦除Flash
                                if (IAP_Erase_App_Sectors(g_firmware_total_size) == 0)
                                {
                                    // 擦除成功，回复肯定应答
                                    ISOTP_Transmit_SF(0x7E8, resp31, sizeof(resp31));

                                    // 状态机
                                    boot_state = STATE_ERASED;
                                }
                                else
                                {
                                    ISOTP_Transmit_SF(0x7E8, resp_31fail, sizeof(resp_31fail));
                                }
                            }
                        }
                    }
                    break;

                    // 36服务，固件数据传输服务
                    case 0x36:
                    {
                        if (boot_state == STATE_ERASED || boot_state == STATE_WRITING)
                        {
                            // 状态机
                            boot_state = STATE_WRITING;

                            // 处理固件信息
                            block_seq = p_data[0];
                            fw_data = &p_data[1];
                            fw_len = data_len - 1;

                            // 计算Flash写入地址
                            write_addr = APPLICATION_START_ADDRESS + g_firmware_bytes_received;

                            // 准备要写入的数据
                            for (j = 0; j < (fw_len / 2); j++)
                            {
                                temp_halfword = (uint16_t)fw_data[2 * j] | ((uint16_t)fw_data[2 * j + 1] << 8);
                                data_to_write[j] = temp_halfword;
                            }

                            if (fw_len % 2 != 0)
                            {
                                temp_halfword = (uint16_t)fw_data[fw_len - 1] | ((uint16_t)0xFF << 8);
                                data_to_write[fw_len / 2] = temp_halfword;
                            }

                            // 关中断
                            __disable_irq();
        
                            if (FLASH_Write_Buffer(write_addr, data_to_write, (fw_len + 1) / 2) == FLASH_COMPLETE)
                            {
                                // 开中断
                                __enable_irq();

                                g_firmware_bytes_received += fw_len;

                                resp36[1] = block_seq;

                                // 写入成功，发送肯定应答
                                ISOTP_Transmit_SF(0x7E8, resp36, 2);
                            }
                            else
                            {
                                // 开中断
                                __enable_irq();

                                resp_36fail[2] = 0x72;

                                // 写入失败，发送否定应答
                                ISOTP_Transmit_SF(0x7E8, resp_36fail, sizeof(resp_36fail));
                            }
                        }
                        else
                        {
                            resp_36fail[2] = 0x24;

                            // 未擦除Flash就写入，发送否定应答
                            ISOTP_Transmit_SF(0x7E8, resp_36fail, sizeof(resp_36fail));
                        }
                    }
                    break;

                    // 37服务，CRC32本地校验
                    case 0x37:
                    {   
                        // 本地软件校验CRC
                        local_crc = IAP_Calculate_CRC_On_Flash(APPLICATION_START_ADDRESS, g_firmware_total_size);

                        // 对比CRC
                        if (local_crc == g_firmware_total_crc)
                        {
                            // 校验成功，发送肯定应答
                            ISOTP_Transmit_SF(0x7E8, resp37, sizeof(resp37));

                            // 重启系统
                            NVIC_SystemReset();
                        }
                        else
                        {
                            // 校验失败，发送否定应答
                            ISOTP_Transmit_SF(0x7E8, resp_37fail, sizeof(resp_37fail));
                        }

                    }
                    break;

                    default:
                    {
                        resp_fail[1] = sid;
                        ISOTP_Transmit_SF(0x7E8, resp_fail, sizeof(resp_fail));
                    }

                }

                // 清空接收状态，准备接收下一个报文
                ISOTP_Init();
            }
            else if (g_isotp.state == ISOTP_RX_ERROR)
            {
                // 出现接收错误，重置接收状态
                ISOTP_Init();
            }            
        }
    }
}



