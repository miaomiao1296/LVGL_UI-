#include "spi_flash.h"
#include "main.h"

extern SPI_HandleTypeDef hspi1;

/* ================= 私有配置 ================= */
#define SPI_TIMEOUT         1000
#define W25Q_SECTOR_SIZE    4096
#define W25Q_BLOCK_SIZE     65536
#define W25Q_PAGE_SIZE      256

// 指令集
#define CMD_WRITE_ENABLE    0x06
#define CMD_READ_STATUS1    0x05
#define CMD_FAST_READ       0x0B
#define CMD_PAGE_PROGRAM    0x02
#define CMD_SECTOR_ERASE    0x20
#define CMD_BLOCK_ERASE     0xD8 

/* ================= 内部状态 ================= */
static volatile bool g_dma_busy = false;;
static BSP_Flash_Callback_t g_user_callback = NULL;

/* ================= 底层辅助 (不暴露) ================= */
static void CS_LOW(void)  { HAL_GPIO_WritePin(SPI_FLASH_CS_GPIO_Port, SPI_FLASH_CS_Pin, GPIO_PIN_RESET); }
static void CS_HIGH(void) { HAL_GPIO_WritePin(SPI_FLASH_CS_GPIO_Port, SPI_FLASH_CS_Pin, GPIO_PIN_SET); }


/* ================= 核心接口实现 ================= */

void BSP_W25Q_Init(void) {
    CS_HIGH();
    HAL_Delay(10);
}

// 1. 同步读取   //Fast Read模式
void BSP_W25Q_Fast_Read(uint8_t* pBuffer, uint32_t ReadAddr, uint32_t Size) {
    if (g_dma_busy) return; // 保护
    
    uint8_t cmd[5] = {CMD_FAST_READ, ReadAddr>>16, ReadAddr>>8, ReadAddr, 0xFF};
    CS_LOW();
    HAL_SPI_Transmit(&hspi1, cmd, 5, 100);
    HAL_SPI_Receive(&hspi1, pBuffer, Size, 1000);
    CS_HIGH();
}

// 最稳定   //Normal Read
void BSP_W25Q_Normal_Read(uint8_t* pBuffer, uint32_t ReadAddr, uint32_t Size) {	
        // 1. 必须等待可能正在进行的图片 DMA 传输完成
    uint32_t timeout = 0xFFFF;
    while (g_dma_busy && timeout--) ;
    
    // 强行把被 DMA 占用的状态机拉回到就绪状态
    hspi1.State = HAL_SPI_STATE_READY;
    hspi1.Lock = HAL_UNLOCKED;

    // 3. 清除硬件溢出标志（碎读最怕这个）
    __HAL_SPI_CLEAR_OVRFLAG(&hspi1);
    __HAL_SPI_ENABLE(&hspi1);

    uint8_t cmd[4] = { 0x03, (uint8_t)(ReadAddr >> 16), (uint8_t)(ReadAddr >> 8), (uint8_t)ReadAddr };

    CS_LOW();
    // 即使是 Normal 读，也要增加一个小的超时控制
    if (HAL_SPI_Transmit(&hspi1, cmd, 4, 10) == HAL_OK) {
        HAL_SPI_Receive(&hspi1, pBuffer, Size, 100);
    }
    CS_HIGH();
}



// 2. 异步DMA读取              在读取极小数据时不要用DMA，比如 4 字节的 Header时存在不稳定性。对于 4 字节的数据，DMA 的初始化、总线仲裁、以及中断处理的时间可能比 SPI 直接传输 4 字节的时间还要长。
void BSP_W25Q_Read_DMA(uint8_t* pBuffer, uint32_t ReadAddr, uint32_t Size, BSP_Flash_Callback_t pCallback) {
    if (g_dma_busy) return; // 防止重入

    // 记录回调
    g_user_callback = pCallback;
    g_dma_busy = true;

    // 启动SPI事务
    CS_LOW();
    
    // CPU发送头部 (5字节极快，阻塞发送更高效)
    uint8_t cmd[5] = {CMD_FAST_READ, ReadAddr>>16, ReadAddr>>8, ReadAddr, 0xFF};
    if (HAL_SPI_Transmit(&hspi1, cmd, 5, 100) != HAL_OK) {
        CS_HIGH();
        g_dma_busy = false;
        return;
    }

    // 启动DMA接收，函数立即返回！
    // CS引脚保持为LOW，直到中断触发
    HAL_SPI_Receive_DMA(&hspi1, pBuffer, Size);
}

bool BSP_W25Q_Is_DMA_Busy(void) {
    return g_dma_busy;
}

// 需要在 main.c 的 HAL_SPI_RxCpltCallback 中调用
void BSP_W25Q_DMA_Callback_Handler(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI1 && g_dma_busy) {
        CS_HIGH(); // 事务结束
        g_dma_busy = false;
        if (g_user_callback) g_user_callback();
    }
}




static void Wait_Flash_Busy(void) {
    uint8_t cmd = CMD_READ_STATUS1;
    uint8_t status;
    uint32_t tick = HAL_GetTick();
    
    CS_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 100);
    do {
        HAL_SPI_Receive(&hspi1, &status, 1, 100);
        if(HAL_GetTick() - tick > SPI_TIMEOUT) break; // 防止死锁
    } while ((status & 0x01) == 0x01);
    CS_HIGH();
}

static void Write_Enable(void) {
    uint8_t cmd = CMD_WRITE_ENABLE;
    CS_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 100);
    CS_HIGH();
}

// 3. 智能写入 (自动处理页边界)
bool BSP_W25Q_Write(uint8_t* pBuffer, uint32_t WriteAddr, uint32_t Size) {
    uint32_t page_remain = W25Q_PAGE_SIZE - (WriteAddr % W25Q_PAGE_SIZE);
    if (Size <= page_remain) page_remain = Size;

    while (1) {
        Write_Enable();
        
        uint8_t cmd[4] = {CMD_PAGE_PROGRAM, WriteAddr>>16, WriteAddr>>8, WriteAddr};
        CS_LOW();
        HAL_SPI_Transmit(&hspi1, cmd, 4, 100);
        HAL_SPI_Transmit(&hspi1, pBuffer, page_remain, 100); // 轮询写数据
        CS_HIGH();
        
        Wait_Flash_Busy(); // 必须等

        if (Size == page_remain) break; // 完成
        
        pBuffer   += page_remain;
        WriteAddr += page_remain;
        Size      -= page_remain;
        
        page_remain = (Size > W25Q_PAGE_SIZE) ? W25Q_PAGE_SIZE : Size;
    }
    return true;
}

// 4. 智能擦除 (自动选择 64KB Block 或 4KB Sector)
bool BSP_W25Q_Erase_Range(uint32_t StartAddr, uint32_t Size) {
    uint32_t end_addr = StartAddr + Size;
    uint32_t current_addr = StartAddr;

    while (current_addr < end_addr) {
        // 算法：优先匹配大块
        // 条件1: 地址是 64K 对齐的        条件2: 剩余长度 >= 64K
        if ((current_addr % W25Q_BLOCK_SIZE == 0) && (end_addr - current_addr >= W25Q_BLOCK_SIZE)) {
            // 执行 64KB 块擦除 (效率最高)
            Write_Enable();
            uint8_t cmd[4] = {CMD_BLOCK_ERASE, current_addr>>16, current_addr>>8, current_addr};
            CS_LOW();
            HAL_SPI_Transmit(&hspi1, cmd, 4, 100);
            CS_HIGH();
            Wait_Flash_Busy();
            current_addr += W25Q_BLOCK_SIZE;
        } 
        else {
            // 否则，执行 4KB 扇区擦除 (处理边缘或小块)
            // 注意：这里需要对齐到 4K 边界，W25Q特性是擦除指令会忽略低12位
            Write_Enable();
            uint8_t cmd[4] = {CMD_SECTOR_ERASE, current_addr>>16, current_addr>>8, current_addr};
            CS_LOW();
            HAL_SPI_Transmit(&hspi1, cmd, 4, 100);
            CS_HIGH();
            Wait_Flash_Busy();
            current_addr += W25Q_SECTOR_SIZE;     // 为了极简，我们按 4K 步进即可，不用纠结起始不对齐导致的重复擦除问题
        }
    }
    return true;
}

