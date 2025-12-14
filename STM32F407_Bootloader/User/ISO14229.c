#include "main.h"

// UDS诊断设备发送至MCU端的CAN ID
#define UDS_PHYSICAL_REQUEST_ID   0x7E0

#define APPLICATION_START_ADDRESS ((uint32_t)0x08008000)

// 定义UDS协议上下文变量
typedef struct {
    uint8_t  current_session;      // 当前会话
    uint8_t  pre_prog_condition;   // 预编程条件标志
    uint32_t s3_server_timer;      // S3 定时器 （暂未实现，预留）
} UDS_Context_t;

static UDS_Context_t g_uds_ctx;

typedef enum {
    STATE_IDLE,
    STATE_ERASED,
    STATE_WRITING
} Boot_State_t;

static Boot_State_t boot_state = STATE_IDLE;

// 固件数据变量
uint32_t g_firmware_total_size = 0;
uint32_t g_firmware_total_crc = 0;
uint32_t g_firmware_bytes_received = 0;
uint8_t  g_expected_packet_num = 0;
static uint16_t data_to_write[2050];
uint32_t write_addr = 0;
uint8_t last_ack_num = 0;
uint8_t block_seq = 0;
uint8_t* fw_data = NULL;
uint16_t fw_len = 0;
uint16_t temp_halfword;
uint32_t i;
uint32_t j;

// CRC校验变量
uint32_t local_crc = 0;

//// UDS否定响应发送函数
//static void UDS_Send_NRC(uint8_t sid, uint8_t nrc)
//{
//    uint8_t resp[3];
//    resp[0] = 0x7F; // Negative Response
//    resp[1] = sid;  // Service ID
//    resp[2] = nrc;  // Error Code
//    ISOTP_Transmit_SF(0x7E8, resp, 3);
//}

// UDS协议初始化函数
void UDS_Init(void)
{
    g_uds_ctx.current_session = UDS_SESSION_DEFAULT;
    g_uds_ctx.pre_prog_condition = 0;
    g_uds_ctx.s3_server_timer = 0;
}

// 传输层接收CAN报文处理函数
void ISO15765_CAN(void)
{
    CanRxMsg msg;
    uint32_t id;

    // 检查环形缓冲器是否有新报文
    if (CAN_RingBuffer_Read(&msg) == 1)
    {
        if (msg.IDE == CAN_Id_Standard)
        {
            id = msg.StdId;
        }
        else
        {
            id = msg.ExtId;
        }
        
        if (id == UDS_PHYSICAL_REQUEST_ID)
        {
            // UDS服务，调用ISO-TP处理
            ISOTP_Receive_Handler(&msg);
        }
    }

}

// UDS应用层处理函数
void UDS_Process(void)
{
    // 检查 ISO-TP 是否收到了完整数据包
    if (ISOTP_IsReceiveComplete() == 1)
    {
        uint8_t* rx_buf = ISOTP_GetRxBuffer();
        uint8_t sid = rx_buf[0];
        uint8_t sub_func = 0;
        uint8_t* p_data = &rx_buf[1];
        uint16_t data_len = ISOTP_GetRxLength() - 1;

        // 取出子功能，34服务没有子功能，就不用解析
        if (ISOTP_GetRxLength() > 1 && sid != 0x34)
        {
            sub_func = rx_buf[1] & 0x7F;
        }

        // 核心业务
        switch (sid)
        {
            // 重启进Bootloader后会再来一次10 02，确认进入Bootloader
            case 0x10:
            {
                if (sub_func == 0x02)
                {
                    if (sub_func == 0x02)
                    {
                        uint8_t resp[] = {0x50, 0x02, 0x00, 0x32, 0x01, 0xF4};
                        ISOTP_Transmit_SF(0x7E8, resp, sizeof(resp));
                        boot_state = STATE_IDLE;
                    }
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
                uint8_t resp[] = {0x74, 0x20, 0x10, 0x00};
                ISOTP_Transmit_SF(0x7E8, resp, sizeof(resp));

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
                        uint8_t resp[] = {0x7F, 0x31, 0x78};
                        ISOTP_Transmit_SF(0x7E8, resp, sizeof(resp));

                        // 根据固件大小擦除Flash
                        if (IAP_Erase_App_Sectors(g_firmware_total_size) == 0)
                        {
                            // 擦除成功，回复肯定应答
                            uint8_t resp[] = {0x71, 0x01, 0xFF, 0x00};
                            ISOTP_Transmit_SF(0x7E8, resp, sizeof(resp));

                            // 状态机
                            boot_state = STATE_ERASED;
                        }
                        else
                        {
                            uint8_t resp[] = {0x7F, 0x31, 0x31};
                            ISOTP_Transmit_SF(0x7E8, resp, sizeof(resp));
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

                        // 写入成功
                        uint8_t resp[] = {0x76, block_seq};
                        ISOTP_Transmit_SF(0x7E8, resp, sizeof(resp));
                    }
                    else
                    {
                        // 开中断
                        __enable_irq();

                        // 写入失败
                        uint8_t resp[] = {0x7F, 0x36, 0x72};
                        ISOTP_Transmit_SF(0x7E8, resp, sizeof(resp));
                    }
                }
                else
                {
                    uint8_t resp[] = {0x7F, 0x36, 0x24};
                    ISOTP_Transmit_SF(0x7E8, resp, sizeof(resp));
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
                    uint8_t resp[] = {0x77};
                    ISOTP_Transmit_SF(0x7E8, resp, sizeof(resp));

                    // 重启系统
                    NVIC_SystemReset();
                }
                else
                {
                    // 校验失败，否定应答
                    uint8_t resp[] = {0x7F, 0x37, 0x72};
                    ISOTP_Transmit_SF(0x7E8, resp, sizeof(resp));
                }
            }
            break;

            default:
            {
                uint8_t resp[] = {0x7F, sid, 0x11};
                ISOTP_Transmit_SF(0x7E8, resp, sizeof(resp));
            }
        }

        ISOTP_Init();
    }
    else if (ISOTP_IsError() == 1)
    {
        ISOTP_Init();
    }
}
