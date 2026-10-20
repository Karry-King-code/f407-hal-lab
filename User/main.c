/*
 * 节点 8 · ADC（HAL 库版）—— 读芯片内部温度 + 内部基准
 *
 * 不用接任何线：
 *   ADC1_IN16 = 芯片温度传感器，ADC1_IN17 = VREFINT(1.21V)
 *
 * ★ TSVREFE（使能温度传感器）在 F4 的 ADC_CCR bit23（F103 在 ADC_CR2！）
 *   HAL 的 HAL_ADC_Init 不管这一位，必须自己置——这是本节点唯一的"手动活"。
 *
 * HAL 代办员：
 *   HAL_ADC_Init + HAL_ADC_ConfigChannel   ← CR1/CR2/SMPR/SQR 全套
 *   HAL_ADC_Start / PollForConversion / GetValue / Stop   ← 启动/等/读
 */

#include "stm32f4xx_hal.h"

/* ============ 串口 ============ */
static void uart1_send(uint8_t b) { while ((USART1->SR & USART_SR_TXE) == 0) { } USART1->DR = b; }
static void uart1_str(const char *s) { while (*s) uart1_send((uint8_t)*s++); }
static void uart1_u32(uint32_t v)
{
    char buf[12]; int i = 0;
    if (v == 0) { uart1_send('0'); return; }
    while (v) { buf[i++] = (char)('0' + v % 10U); v /= 10U; }
    while (i) uart1_send((uint8_t)buf[--i]);
}

static void uart_init(void)
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

/* ============ ADC1（HAL） ============ */
ADC_HandleTypeDef hadc1;

void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    __HAL_RCC_ADC1_CLK_ENABLE();
}

static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV2;  /* 16/2 = 8MHz */
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;   /* 软件触发 */
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) { while (1) { } }

    /* ★ 唯一手动活：使能温度传感器/VREFINT（ADC_CCR bit23） */
    ADC->CCR |= ADC_CCR_TSVREFE;
    HAL_Delay(10);                        /* 传感器启动时间 */

    sConfig.Channel      = ADC_CHANNEL_16;   /* 温度传感器 */
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    sConfig.Offset       = 0;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

static uint16_t adc_read(uint32_t channel)
{
    ADC_ChannelConfTypeDef cfg = {0};
    uint16_t v;

    cfg.Channel      = channel;
    cfg.Rank         = 1;
    cfg.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    cfg.Offset       = 0;
    HAL_ADC_ConfigChannel(&hadc1, &cfg);

    HAL_ADC_Start(&hadc1);                          /* 启动转换 */
    HAL_ADC_PollForConversion(&hadc1, 100);         /* 等转换完成（带超时） */
    v = (uint16_t)HAL_ADC_GetValue(&hadc1);         /* 取结果 */
    HAL_ADC_Stop(&hadc1);
    return v;
}

int main(void)
{
    uint16_t raw_t, raw_vref;
    uint32_t mv_t, mv_vref, temp_x10;

    HAL_Init();
    uart_init();
    MX_ADC1_Init();

    uart1_str("\r\n=== ADC INTERNAL TEST (HAL) ===\r\n");
    uart1_str("(touch the chip, temperature rises)\r\n");

    while (1)
    {
        raw_t    = adc_read(ADC_CHANNEL_16);        /* 温度传感器 */
        raw_vref = adc_read(ADC_CHANNEL_VREFINT);   /* 内部基准 */

        mv_t    = (uint32_t)raw_t    * 3300UL / 4095UL;
        mv_vref = (uint32_t)raw_vref * 3300UL / 4095UL;
        temp_x10 = (uint32_t)((760 - (int32_t)mv_t) * 4 + 250);

        uart1_str("TEMP_RAW=");
        uart1_u32(raw_t);
        uart1_str("  mV=");
        uart1_u32(mv_t);
        uart1_str("  T=");
        uart1_u32(temp_x10 / 10);
        uart1_send('.');
        uart1_u32(temp_x10 % 10);
        uart1_str("C   VREF_RAW=");
        uart1_u32(raw_vref);
        uart1_str("  mV=");
        uart1_u32(mv_vref);
        uart1_str("\r\n");

        HAL_Delay(500);
    }
}
