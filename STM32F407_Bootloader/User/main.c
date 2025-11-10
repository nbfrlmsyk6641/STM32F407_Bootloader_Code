#include "main.h" 

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
                    
                // 状态3: 根据固件数据擦除Flash
                case IAP_STATE_ERASE_FLASH: 
                    
                    if (IAP_Erase_App_Sectors(g_firmware_total_size) == 0)
                    {
                        // 擦除成功，发送“擦除完毕”响应 (0xB1)
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
                    
                // 状态4: 等待固件数据包
                case IAP_STATE_WAIT_DATA:
                    // TODO: 在这里实现接收0xC2数据包的逻辑
                    // 1. 检查 if (rx_id == HOST_REQUEST_ID_DATA)
                    // 2. 检查数据头 Data[0] == 0xAA
                    // 3. 检查序列号 Data[1] == g_expected_packet_num
                    // 4. (如果OK) 写入Flash: FLASH_Write_Buffer(...)
                    // 5. (如果OK) 发送ACK: CAN1_Transmit_TX(0xB2, 1, &g_expected_packet_num)
                    // 6. (如果OK) g_expected_packet_num++
                    // 7. (如果OK) g_firmware_bytes_received += 6 (数据长度)
                    // 8. (如果OK) 检查 if (g_firmware_bytes_received >= g_firmware_total_size) -> g_iap_state = IAP_STATE_WAIT_EOT;
                    // 9. (如果序列号错误) 重复发送上一个ACK
                    if(rx_id == 0xC2)
                    {
                        IAP_Timeout_Counter = 0;
                    }
                    else if (IAP_Timeout_Counter >= 10)
                    {
                        // 超过10秒未收到固件数据包，进入错误状态
                        g_iap_state = IAP_STATE_FAILURE;
                    }
                    break;
                    
                // 状态5: 等待传输结束 (0xC3)
                case IAP_STATE_WAIT_EOT:
                    // TODO: 在这里实现等待0xC3 EOT报文的逻辑
                    // 1. 检查 if (rx_id == HOST_REQUEST_ID_EOT)
                    // 2. 检查数据头 Data[0] == 0xBB
                    // 3. (如果OK) -> g_iap_state = IAP_STATE_VERIFY;
                    break;
                    
                // 状态6: 执行CRC校验
                case IAP_STATE_VERIFY:
                    // TODO: 在这里实现CRC校验逻辑
                    // 1. local_crc = Calculate_CRC_On_Flash(g_firmware_total_size);
                    // 2. if (local_crc == g_firmware_total_crc) -> g_iap_state = IAP_STATE_SUCCESS;
                    // 3. else -> g_iap_state = IAP_STATE_FAILURE;
                    break;
                    
                // 状态7: 更新成功
                case IAP_STATE_SUCCESS:
                    // 发送最终成功报文 (0xB3)
                    // CAN1_Transmit_TX(0xB3, 8, ...);
                    // Delay(100); // 确保报文发送出去
                    // NVIC_SystemReset(); // 重启进入新App
                    break;
                    
                // 状态8: 更新失败 (CRC错误或大小错误)
                case IAP_STATE_FAILURE:

                    // 通知上位机更新失败，并准备好重试
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



