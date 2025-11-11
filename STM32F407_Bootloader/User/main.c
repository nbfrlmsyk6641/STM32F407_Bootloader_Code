#include "main.h" 

#define APPLICATION_START_ADDRESS ((uint32_t)0x08008000)

// 定义软件更新状态机枚举变量
typedef enum 
{
    IAP_STATE_IDLE,             // 0: 空闲 
    IAP_STATE_START,            // 1: IAP启动，发送“准备就绪”
    IAP_STATE_WAIT_METADATA,    // 2: 等待上位机发送元数据 (0xC1)
    IAP_STATE_ERASE_FLASH,      // 3: 正在擦除Flash
    IAP_STATE_WAIT_DATA,        // 4: 等待固件数据包 (0xC2)
    IAP_STATE_WAIT_EOT,         // 5: 等待传输结束指令 (0xC3)
    IAP_STATE_VERIFY,           // 6: 正在执行CRC校验
    IAP_STATE_SUCCESS,          // 7: 更新成功，准备重启
    IAP_STATE_FAILURE           // 8: 更新失败，等待重试
} IAP_State_t;

// 无升级时发送的报文变量
uint32_t Normal_TxID = 0xB0;
uint8_t Normal_TxLength = 8;
uint8_t Normal_TxData[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// 有升级时发送的报文变量

// 应答上位机的报文，确认存在升级请求
uint32_t Boot_Confirm_TxID = 0xB0;
uint8_t Boot_Confirm_TxLength = 8;
uint8_t Boot_Confirm_TxData[8] = {0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22};

// 应答上位机的报文，答复Flash已擦除
uint32_t Boot_Erase_TxID = 0xB1;
uint8_t Boot_Erase_TxLength = 8;
uint8_t Boot_Erase_TxData[8] = {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33};

// 应答上位机的报文，通知收到一帧数据包
uint32_t Boot_DataAck_TxID = 0xB2;
uint8_t Boot_DataAck_TxLength = 8;
uint8_t Boot_DataAck_TxData[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// 应答上位机的报文，通知更新成功
uint32_t Boot_Success_TxID = 0xB3;
uint8_t Boot_Success_TxLength = 8;
uint8_t Boot_Success_TxData[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// 应答上位机的报文，通知更新失败，准备重试
uint32_t Boot_Error_TxID = 0xB4;
uint8_t Boot_Error_TxLength = 8;
uint8_t Boot_Error_TxData[8] = {0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44};

// IAP标志位
uint8_t IAP_Flag = 0;

// IAP状态机
IAP_State_t g_iap_state = IAP_STATE_IDLE;

// IAP计数变量
uint8_t IAP_Timeout_Counter = 0;

// BKP变量
uint32_t BKP_Data = 0;

// 固件数据变量
uint32_t g_firmware_total_size = 0;
uint32_t g_firmware_total_crc = 0;
uint32_t g_firmware_bytes_received = 0;
uint8_t  g_expected_packet_num = 0;
uint16_t data_to_write[3];
uint32_t write_addr = 0;
uint8_t last_ack_num = 0;

// CRC校验变量
uint32_t local_crc = 0;
uint32_t g_debug_failed_crc = 0;

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

        // 存在升级请求
        IAP_Flag = 1;
    }
    else
    {
        // 不存在升级请求
        IAP_Flag = 0;
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

                // 每1s发送一帧CAN报文
                CAN1_Transmit_TX(Normal_TxID, Normal_TxLength, Normal_TxData);
            }

            if(Get_Task2_Flag() == 1)
            {
                // 5秒等待结束，无任何特殊请求，跳转到App
                Clear_Task2_Flag();

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

        // 进入IAP状态机，启动IAP流程
        g_iap_state = IAP_STATE_START;

        while(1)
        {
            // CAN报文接收缓冲
            uint32_t rx_id;
            uint8_t  rx_len;
            uint8_t  rx_data[8];

            // LED闪烁，指示正在运行IAP
            if(Get_Task1_Flag() == 1)
            {
                // 5s等待过程中
                Clear_Task1_Flag();

                // 持续闪烁LED且频率高于App的LED闪烁频率
                GPIO_ToggleBits(GPIOE, GPIO_Pin_13);
            }

            // IAP超时计数
            if(Get_Task3_Flag() == 1)
            {
                // 1s计数
                Clear_Task3_Flag();

                IAP_Timeout_Counter++;
            }

            // CAN报文接收
            if(CAN1_ReceiveFlag() == 1)
            {
                CAN1_Receive_RX(&rx_id, &rx_len, rx_data);
            }
            else
            {   
                // 无报文接收，清一下ID，避免干扰状态机
                rx_id = 0;
            }

            // IAP状态机处理流程
            switch (g_iap_state)
            {
                // 状态1: IAP启动，发送“准备就绪”
                case IAP_STATE_START:

                    // 发送“Bootloader就绪”报文 (0xB0)，通知上位机
                    CAN1_Transmit_TX(Boot_Confirm_TxID, Boot_Confirm_TxLength, Boot_Confirm_TxData);
                    
                    // 立即切换到下一个状态：等待固件数据
                    g_iap_state = IAP_STATE_WAIT_METADATA;
                    
                    // 重置超时计数器
                    IAP_Timeout_Counter = 0;
                    
                    break;

                // 状态2: 等待上位机发送固件数据 (0xC1)
                case IAP_STATE_WAIT_METADATA:

                    if (rx_id == 0xC1 && rx_len == 8) // ID 0xC1
                    {
                        // 1. 解包固件大小和CRC校验码
                        g_firmware_total_size = (uint32_t)rx_data[0] |
                                                (uint32_t)(rx_data[1] << 8) |
                                                (uint32_t)(rx_data[2] << 16) |
                                                (uint32_t)(rx_data[3] << 24);
                                        
                        g_firmware_total_crc =  (uint32_t)rx_data[4] |
                                                (uint32_t)(rx_data[5] << 8) |
                                                (uint32_t)(rx_data[6] << 16) |
                                                (uint32_t)(rx_data[7] << 24);

                        // 2. 校验大小
                        if (g_firmware_total_size == 0 || g_firmware_total_size > (300 * 1024))
                        {
                            // 固件太大或为0，校验失败，进入错误状态
                            g_iap_state = IAP_STATE_FAILURE;
                        }
                        else
                        {
                            // 校验成功，进入擦除状态
                            g_iap_state = IAP_STATE_ERASE_FLASH;
                        }
                    }
                    else if (IAP_Timeout_Counter >= 10)
                    {
                        // 超过10秒未收到固件数据，进入错误状态
                        g_iap_state = IAP_STATE_FAILURE;
                    }

                    break;
                    
                // 状态3: 根据固件大小擦除Flash
                case IAP_STATE_ERASE_FLASH: 
                    
                    if (IAP_Erase_App_Sectors(g_firmware_total_size) == 0)
                    {
                        // 擦除成功，发送“擦除完毕”响应 (0xB1)至上位机
                        CAN1_Transmit_TX(Boot_Erase_TxID, Boot_Erase_TxLength, Boot_Erase_TxData);
                        
                        // 切换到下一个状态
                        g_firmware_bytes_received = 0;
                        g_expected_packet_num = 0;
                        g_iap_state = IAP_STATE_WAIT_DATA;
                        IAP_Timeout_Counter = 0; 
                    }
                    else
                    {
                        // 擦除失败
                        g_iap_state = IAP_STATE_FAILURE;
                    }

                    break;
                    
                // 状态4: 等待固件数据包，并将数据写入Flash
                case IAP_STATE_WAIT_DATA:

                    if(rx_id == 0xC2 && rx_len == 8)
                    {
                        if(rx_data[0] != 0xAA)
                        {
                            // 数据包起始标志错误，进入错误状态
                            g_iap_state = IAP_STATE_FAILURE;
                            break;
                        }

                        if(rx_data[1] == g_expected_packet_num)
                        {   
                            // 1. 重置超时计数器
                            IAP_Timeout_Counter = 0;

                            // 2. 从数据包中拆出要写入的6个字节
                            data_to_write[0] = (uint16_t)rx_data[2] | ((uint16_t)rx_data[3] << 8);
                            data_to_write[1] = (uint16_t)rx_data[4] | ((uint16_t)rx_data[5] << 8);
                            data_to_write[2] = (uint16_t)rx_data[6] | ((uint16_t)rx_data[7] << 8);

                            // 3. 计算写入地址
                            write_addr = APPLICATION_START_ADDRESS + g_firmware_bytes_received;

                            // 4. 写入Flash
                            if (FLASH_Write_Buffer(write_addr, data_to_write, 3) == FLASH_COMPLETE)
                            {
                                // 5. 写入成功，发送应答报文至上位机
                                Boot_DataAck_TxData[0] = g_expected_packet_num; // 将序列号放入ACK报文
                                CAN1_Transmit_TX(Boot_DataAck_TxID, Boot_DataAck_TxLength, Boot_DataAck_TxData);
                                
                                // 6. 更新计数变量状态
                                g_expected_packet_num ++; // 期待下一个包 (0-255 自动回绕)
                                g_firmware_bytes_received += 6; // 每包固定传输6字节
                                
                                // 7. 检查是否全部完成
                                //    (注意：这里用 >= 是为了处理最后
                                //     一包 host 补 0xFF 的情况)
                                if (g_firmware_bytes_received >= g_firmware_total_size)
                                {
                                    // 所有数据包已接收完毕
                                    g_iap_state = IAP_STATE_WAIT_EOT;
                                    IAP_Timeout_Counter = 0; // 重置超时，准备等待EOT
                                }
                            }
                            else
                            {
                                // Flash 写入失败
                                g_iap_state = IAP_STATE_FAILURE;
                            }
                        }
                        else if (rx_data[1] < g_expected_packet_num) // 收到一个重复的旧包
                        {
                            // 1. 重置超时
                            IAP_Timeout_Counter = 0;
                            
                            // 2. 重新发送上一个的ACK
                            last_ack_num = (g_expected_packet_num == 0) ? 0xFF : (g_expected_packet_num - 1);
                            Boot_DataAck_TxData[0] = last_ack_num;
                            CAN1_Transmit_TX(Boot_DataAck_TxID, Boot_DataAck_TxLength, Boot_DataAck_TxData);
                        }
                    }
                    else if (IAP_Timeout_Counter >= 5) // 5秒超时
                    {
                        g_iap_state = IAP_STATE_FAILURE;
                    }

                    break;
                    
                // 状态5: 等待上位机指示传输结束 (0xC3)
                case IAP_STATE_WAIT_EOT:

                    if(rx_id == 0xC3 && rx_len == 8 && rx_data[0] == 0xBB) // 0xBB 是EOT包头
                    {
                        IAP_Timeout_Counter = 0;
                        g_iap_state = IAP_STATE_VERIFY; // 进入校验状态
                    }
                    else if (IAP_Timeout_Counter >= 5) // 5秒EOT超时
                    {
                        g_iap_state = IAP_STATE_FAILURE;
                    }

                    break;
                    
                // 状态6: 执行CRC校验
                case IAP_STATE_VERIFY:

                    // 1. 调用硬件CRC计算Flash中的固件
                    local_crc = IAP_Calculate_CRC_On_Flash(APPLICATION_START_ADDRESS, g_firmware_total_size);
                    
                    // 2. 对比CRC
                    if (local_crc == g_firmware_total_crc)
                    {
                        // 校验成功！
                        g_iap_state = IAP_STATE_SUCCESS;
                    }
                    else
                    {
                        // 校验失败！
                        g_debug_failed_crc = local_crc;
                        g_iap_state = IAP_STATE_FAILURE;
                    }

                    break;
                    
                // 状态7: 更新成功
                case IAP_STATE_SUCCESS:

                    // 1. 发送最终成功报文 (0xB3)
                    CAN1_Transmit_TX(Boot_Success_TxID, Boot_Success_TxLength, Boot_Success_TxData);
                    
                    // 2. 软件复位系统，进入新App
                    NVIC_SystemReset();

                    break;
                    
                // 状态8: 更新失败 (CRC错误或大小错误)
                case IAP_STATE_FAILURE:

                    // 通知上位机更新失败，并准备好重试

                    // 按小端模式打包 (匹配Python的 <I )
                    Boot_Error_TxData[0] = (uint8_t)(g_debug_failed_crc);
                    Boot_Error_TxData[1] = (uint8_t)(g_debug_failed_crc >> 8);
                    Boot_Error_TxData[2] = (uint8_t)(g_debug_failed_crc >> 16);
                    Boot_Error_TxData[3] = (uint8_t)(g_debug_failed_crc >> 24);
                    
                    // (你也可以把Python期望的CRC放在后4字节)
                    Boot_Error_TxData[4] = (uint8_t)(g_firmware_total_crc);
                    Boot_Error_TxData[5] = (uint8_t)(g_firmware_total_crc >> 8);
                    Boot_Error_TxData[6] = (uint8_t)(g_firmware_total_crc >> 16);
                    Boot_Error_TxData[7] = (uint8_t)(g_firmware_total_crc >> 24);
                
                    CAN1_Transmit_TX(Boot_Error_TxID, Boot_Error_TxLength, Boot_Error_TxData);

                    // 切换回等待固件数据状态，以便上位机重新发起流程
                    g_iap_state = IAP_STATE_START;
                    IAP_Timeout_Counter = 0;

                    break;

                // 默认状态 (异常处理)
                default:
                    // 发生了未知状态，重置流程
                    g_iap_state = IAP_STATE_FAILURE;
                    break;

            } // end switch
        }
    }
}



