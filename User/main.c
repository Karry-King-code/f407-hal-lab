/*
 * 节点 4 · I2C 读写 EEPROM（AT24C02）—— HAL 库版
 *
 * 硬件事实与位带版相同：AT24C02 SCL=PB10 SDA=PB11（硬件 I2C2 引脚，AF4，开漏），
 * 器件地址 0xA0(写)/0xA1(读)。
 *
 * HAL 的代办员：HAL_I2C_Mem_Write / HAL_I2C_Mem_Read
 *   —— 起始位/寻址/应答/停止位全由 HAL 的状态机代打（位带版你亲手拉的每一拍它都包了）
 *   —— DevAddress 直接填 0xA0（HAL 内部只发高 7 位）
 *
 * ★ 与位带版同样的坑：EEPROM 写完有最长 5ms 内部编程周期，期间一切请求 NAK。
 *   HAL 不会替你等——写和读之间必须 HAL_Delay(5)。
 *
 * 功能：向 0 号格子写 0x5A → 读回 → 串口打印 PASS
 */

#include "stm32f4xx_hal.h"

/* ============ 串口（节点三原样搬来） ============ */

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

/* ============ I2C2（HAL） ============ */

I2C_HandleTypeDef hi2c2;

static void MX_I2C2_Init(void);

/* HAL_I2C_Init 会自动调的"芯片杂活"回调：时钟/引脚（阻塞模式不需要 NVIC） */
void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C2_CLK_ENABLE();

    gpio.Pin       = GPIO_PIN_10 | GPIO_PIN_11;
    gpio.Mode      = GPIO_MODE_AF_OD;            /* ★开漏复用——I2C 必须开漏（位带版第一层洋葱） */
    gpio.Pull      = GPIO_NOPULL;                /* 板上有外部 10K 上拉 */
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF4_I2C2;
    HAL_GPIO_Init(GPIOB, &gpio);
}

static void MX_I2C2_Init(void)
{
    hi2c2.Instance               = I2C2;
    hi2c2.Init.ClockSpeed        = 100000;             /* 100kHz */
    hi2c2.Init.DutyCycle         = I2C_DUTYCYCLE_2;
    hi2c2.Init.OwnAddress1       = 0;                  /* 我们是主机，从机地址随便 */
    hi2c2.Init.AddressingMode    = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode   = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.GeneralCallMode   = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode     = I2C_NOSTRETCH_DISABLE;  /* 允许从机拉钟 */
    if (HAL_I2C_Init(&hi2c2) != HAL_OK)                /* CR2/CCR/TRISE/PE 全由它算好写入 */
    {
        while (1) { }
    }
}

/* ============ 主线 ============ */

int main(void)
{
    uint8_t tx = 0x5A;
    uint8_t rd = 0;

    HAL_Init();
    uart_init();
    MX_I2C2_Init();

    uart1_str("\r\n=== I2C2 AT24C02 TEST (HAL) ===\r\n");

    HAL_I2C_Mem_Write(&hi2c2, 0xA0, 0x00, I2C_MEMADD_SIZE_8BIT, &tx, 1, 100);
    HAL_Delay(5);                                /* ★等内部编程周期（位带版第四层洋葱） */
    if (HAL_I2C_Mem_Read(&hi2c2, 0xA0, 0x00, I2C_MEMADD_SIZE_8BIT, &rd, 1, 100) != HAL_OK)
        rd = 0xEE;                               /* 读失败给个特征值 */

    uart1_str("WR=0x5A  RD=0x");
    uart1_hex(rd);
    if (rd == 0x5A)
        uart1_str("  PASS\r\n");
    else
        uart1_str("  FAIL\r\n");

    while (1) { }
}
