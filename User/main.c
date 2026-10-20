/*
 * 节点 5 · SPI 读写 Flash（W25Q16）—— HAL 库版
 *
 * 硬件事实同寄存器版：CS=PA4（GPIO）、SCK=PA5、MISO=PA6、MOSI=PA7（SPI1，AF5），模式 0
 * 功能同样三段：读 JEDEC ID → 扇区擦除 → 页编程 "F407" → 读回比对
 *
 * HAL 代办员：
 *   HAL_SPI_Transmit(&hspi1, buf, len, timeout)  只发
 *   HAL_SPI_Receive (&hspi1, buf, len, timeout)  只收（内部自动发哑字节造时钟）
 *   HAL_SPI_TransmitReceive(&hspi1, tx, rx, len, timeout)  边发边收
 *   —— 寄存器版要手写的"等 TXE/等 RXNE"，HAL 全包在函数里
 */

#include "stm32f4xx_hal.h"

/* ============ 串口 ============ */

void uart1_send(uint8_t b)
{
    while ((USART1->SR & USART_SR_TXE) == 0) { }
    USART1->DR = b;
}

void uart1_str(const char *s)
{
    while (*s)
        uart1_send((uint8_t)*s++);
}

void uart1_hex(uint8_t v)
{
    const char h[] = "0123456789ABCDEF";
    uart1_send((uint8_t)h[v >> 4]);
    uart1_send((uint8_t)h[v & 0x0F]);
}

void uart_init(void)
{
    SystemCoreClockUpdate();

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    GPIOA->MODER &= ~(3UL << 18);  GPIOA->MODER |= (2UL << 18);
    GPIOA->MODER &= ~(3UL << 20);  GPIOA->MODER |= (2UL << 20);
    GPIOA->AFR[1] &= ~(0xFUL << 4);  GPIOA->AFR[1] |= (7UL << 4);
    GPIOA->AFR[1] &= ~(0xFUL << 8);  GPIOA->AFR[1] |= (7UL << 8);

    USART1->BRR = 0x8B;
    USART1->CR1 |= USART_CR1_UE;
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE;
}

/* ============ SPI1（HAL） ============ */

SPI_HandleTypeDef hspi1;

#define CS_L()   (GPIOA->BSRR = (1UL << 4))
#define CS_H()   (GPIOA->BSRR = (1UL << 4) << 16)

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();

    /* PA5/PA6/PA7 = SPI1 复用 AF5 */
    gpio.Pin       = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* PA4 = 普通推挽输出（片选，软件管） */
    gpio.Pin       = GPIO_PIN_4;
    gpio.Mode      = GPIO_MODE_OUTPUT_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    CS_H();
}

static void MX_SPI1_Init(void)
{
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;   /* 全双工 */
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;       /* 模式 0 */
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;        /* 模式 0 */
    hspi1.Init.NSS               = SPI_NSS_SOFT;           /* 软件片选 */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;/* 16MHz/4 = 4MHz */
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial     = 7;
    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        while (1) { }
    }
}

static uint8_t spi_txrx(uint8_t b)         /* 寄存器版的 4 行 → HAL 一行 */
{
    uint8_t r;
    HAL_SPI_TransmitReceive(&hspi1, &b, &r, 1, 100);
    return r;
}

/* ============ W25Q16 协议层 ============ */

static void flash_write_enable(void)
{
    uint8_t cmd = 0x06;
    CS_L();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 100);
    CS_H();
}

static uint8_t flash_wait_busy(void)
{
    uint32_t t = 1000000;
    uint8_t sr = 0;
    CS_L();
    spi_txrx(0x05);
    do {
        sr = spi_txrx(0xFF);
    } while ((sr & 0x01) && --t);
    CS_H();
    return t ? 1 : 0;
}

static uint32_t flash_read_jedec(void)
{
    uint32_t id;
    uint8_t cmd = 0x9F;
    uint8_t b[3];
    CS_L();
    HAL_SPI_TransmitReceive(&hspi1, &cmd, b, 1, 100);  /* 命令字节的返回值是垃圾，丢掉 */
    HAL_SPI_Receive(&hspi1, b, 3, 100);                /* 再收 3 字节 = JEDEC ID */
    CS_H();
    id = ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | b[2];
    return id;
}

static uint8_t flash_erase_sector(void)
{
    uint8_t cmd[4] = {0x20, 0x00, 0x00, 0x00};
    flash_write_enable();
    CS_L();
    HAL_SPI_Transmit(&hspi1, cmd, 4, 100);
    CS_H();
    return flash_wait_busy();
}

static uint8_t flash_prog_page(const uint8_t *d, uint8_t len)
{
    uint8_t cmd[4] = {0x02, 0x00, 0x00, 0x00};
    flash_write_enable();
    CS_L();
    HAL_SPI_Transmit(&hspi1, cmd, 4, 100);
    HAL_SPI_Transmit(&hspi1, (uint8_t *)d, len, 100);
    CS_H();
    return flash_wait_busy();
}

static void flash_read_data(uint8_t *out, uint8_t len)
{
    uint8_t cmd[4] = {0x03, 0x00, 0x00, 0x00};
    CS_L();
    HAL_SPI_Transmit(&hspi1, cmd, 4, 100);
    HAL_SPI_Receive(&hspi1, out, len, 100);   /* 只收：HAL 自动发哑字节造时钟 */
    CS_H();
}

/* ============ 主线 ============ */

int main(void)
{
    uint32_t id;
    uint8_t rd[4] = {0};
    const uint8_t wr[4] = {'F', '4', '0', '7'};
    uint8_t i, ok = 1;

    HAL_Init();
    uart_init();
    MX_SPI1_Init();

    uart1_str("\r\n=== SPI1 W25Q16 TEST (HAL) ===\r\n");

    id = flash_read_jedec();
    uart1_str("JEDEC=0x");
    uart1_hex((uint8_t)(id >> 16));
    uart1_hex((uint8_t)(id >> 8));
    uart1_hex((uint8_t)id);
    if (id == 0xEF4015UL) uart1_str("  ID-PASS\r\n");
    else                  uart1_str("  ID-FAIL\r\n");

    ok &= flash_erase_sector();
    ok &= flash_prog_page(wr, 4);
    flash_read_data(rd, 4);

    uart1_str("READ=");
    for (i = 0; i < 4; i++)
        uart1_send(rd[i]);
    uart1_str("  ");
    for (i = 0; i < 4; i++)
        if (rd[i] != wr[i]) ok = 0;
    if (ok) uart1_str("PASS\r\n");
    else    uart1_str("FAIL\r\n");

    while (1) { }
}
