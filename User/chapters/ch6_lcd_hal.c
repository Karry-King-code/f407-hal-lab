/*
 * 节点 6 · 屏幕 ST7789（HAL 库版）
 *
 * 硬件事实与寄存器版相同：
 *   SCK=PB3  MOSI=PB5（SPI1 PB组 AF5）、DC=PB0、CS=PA15、RST=PC7、LED=PB1
 *   实测：本屏必须 INVOFF(0x20)，开了 INVON 会红绿蓝全反色
 *
 * HAL 代办员：
 *   HAL_SPI_Transmit(&hspi1, buf, len, timeout)  ← 寄存器版要循环等 TXE 写 DR
 *   HAL_GPIO_WritePin(...)                        ← CS/DC/RST 电平
 * 亮点：填充整屏用 512 字节 buffer 批量发送，一次调用发一大片（寄存器版是逐字节）
 */

#include "stm32f4xx_hal.h"

#define CS_L()    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET)
#define CS_H()    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET)
#define DC_L()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET)
#define DC_H()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET)
#define RST_L()   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET)
#define RST_H()   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET)
#define LED_ON()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET)

SPI_HandleTypeDef hspi1;

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();

    /* PB3/PB4/PB5 = SPI1 AF5（PB3/PB4 是 JTAG 脚，配 AF 即释放；SWD 不受影响） */
    gpio.Pin       = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOB, &gpio);

    /* DC=PB0, LED=PB1 */
    gpio.Pin   = GPIO_PIN_0 | GPIO_PIN_1;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    /* CS=PA15 */
    gpio.Pin  = GPIO_PIN_15;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* RST=PC7 */
    gpio.Pin  = GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOC, &gpio);

    CS_H();
    DC_H();
}

static void MX_SPI1_Init(void)
{
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;   /* 16/2 = 8MHz */
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial     = 7;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) { while (1) { } }
}

/* ============ ST7789 ============ */
static void lcd_cmd(uint8_t c)
{
    CS_L(); DC_L();
    HAL_SPI_Transmit(&hspi1, &c, 1, 100);
    CS_H();
}

static void lcd_data(uint8_t d)
{
    CS_L(); DC_H();
    HAL_SPI_Transmit(&hspi1, &d, 1, 100);
    CS_H();
}

static void lcd_reset(void)
{
    RST_H(); HAL_Delay(10);
    RST_L(); HAL_Delay(20);
    RST_H(); HAL_Delay(120);
}

static void lcd_init(void)
{
    lcd_reset();

    lcd_cmd(0x01); HAL_Delay(150);
    lcd_cmd(0x11); HAL_Delay(120);
    lcd_cmd(0x3A); lcd_data(0x55);     /* RGB565 */
    lcd_cmd(0x36); lcd_data(0x00);     /* 方向 */
    lcd_cmd(0x20);                     /* 反色关（实测本屏必须关） */
    lcd_cmd(0x13);
    lcd_cmd(0x29); HAL_Delay(20);
}

static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t b[4];
    lcd_cmd(0x2A);
    b[0] = x0 >> 8; b[1] = x0 & 0xFF; b[2] = x1 >> 8; b[3] = x1 & 0xFF;
    CS_L(); DC_H();
    HAL_SPI_Transmit(&hspi1, b, 4, 100);   /* 4 字节一次发完（寄存器版要 4 次） */
    CS_H();

    lcd_cmd(0x2B);
    b[0] = y0 >> 8; b[1] = y0 & 0xFF; b[2] = y1 >> 8; b[3] = y1 & 0xFF;
    CS_L(); DC_H();
    HAL_SPI_Transmit(&hspi1, b, 4, 100);
    CS_H();

    lcd_cmd(0x2C);
}

static void lcd_fill(uint16_t color)
{
    static uint8_t buf[512];
    uint32_t i, remain = 240UL * 320UL * 2UL;   /* 总字节数 */
    uint16_t chunk = 0;

    for (i = 0; i < sizeof(buf); i += 2)        /* 把颜色填满 buffer */
    {
        buf[i]     = (uint8_t)(color >> 8);
        buf[i + 1] = (uint8_t)(color & 0xFF);
    }

    lcd_set_window(0, 0, 239, 319);
    CS_L(); DC_H();
    while (remain)
    {
        chunk = (remain > sizeof(buf)) ? sizeof(buf) : (uint16_t)remain;
        HAL_SPI_Transmit(&hspi1, buf, chunk, 500);   /* 512 字节一次调用 */
        remain -= chunk;
    }
    CS_H();
}

int main(void)
{
    HAL_Init();
    MX_SPI1_Init();
    LED_ON();                       /* 背光 */
    lcd_init();

    while (1)
    {
        lcd_fill(0xF800); HAL_Delay(1000);   /* 红 */
        lcd_fill(0x07E0); HAL_Delay(1000);   /* 绿 */
        lcd_fill(0x001F); HAL_Delay(1000);   /* 蓝 */
    }
}
