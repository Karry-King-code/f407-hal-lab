/*
 * 节点 9 · DAC（HAL 库版）—— 输出 + ADC 内部回读自测
 *
 * 硬件：DAC_OUT1 = PA4（同时是 ADC1_IN4，也是 Flash 的 CS）
 * 自测：写 DAC，立刻用 ADC 读回同一根脚，两个值应接近
 *
 * HAL 代办员：
 *   HAL_DAC_Init + HAL_DAC_ConfigChannel     ← DAC_CR / 触发源 / 输出缓冲
 *   HAL_DAC_SetValue(&hdac, CH1, ALIGN_12B_R, v)   ← 写 DHR12R1
 *   HAL_DAC_Start(&hdac, CH1)                ← 使能通道
 * ADC 回读仍走节点八的 HAL ADC
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

/* ============ DAC（HAL） ============ */
DAC_HandleTypeDef hdac;

void HAL_DAC_MspInit(DAC_HandleTypeDef *hdac)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_DAC_CLK_ENABLE();

    gpio.Pin  = GPIO_PIN_4;
    gpio.Mode = GPIO_MODE_ANALOG;      /* PA4 模拟模式（DAC/ADC 共用） */
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);
}

static void MX_DAC_Init(void)
{
    DAC_ChannelConfTypeDef sConfig = {0};

    hdac.Instance = DAC;
    if (HAL_DAC_Init(&hdac) != HAL_OK) { while (1) { } }

    sConfig.DAC_Trigger      = DAC_TRIGGER_NONE;            /* 软件写值立即输出 */
    sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
    if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK) { while (1) { } }

    HAL_DAC_Start(&hdac, DAC_CHANNEL_1);
}

/* ============ ADC（HAL，回读 PA4） ============ */
ADC_HandleTypeDef hadc1;

void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    __HAL_RCC_ADC1_CLK_ENABLE();
}

static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) { while (1) { } }

    sConfig.Channel      = ADC_CHANNEL_4;          /* PA4 */
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    sConfig.Offset       = 0;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

static uint16_t adc_read_ch4(void)
{
    uint16_t v;
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 100);
    v = (uint16_t)HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return v;
}

int main(void)
{
    static const uint16_t steps[3] = {0, 2048, 4095};
    uint8_t i;
    uint16_t set, got;
    uint32_t mv_set, mv_got;

    HAL_Init();
    uart_init();
    MX_DAC_Init();
    MX_ADC1_Init();

    uart1_str("\r\n=== DAC LOOPBACK TEST (HAL) ===\r\n");
    uart1_str("DAC_OUT1=PA4 (also ADC1_IN4)\r\n");

    while (1)
    {
        for (i = 0; i < 3; i++)
        {
            set = steps[i];
            HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, set);
            HAL_Delay(20);
            got = adc_read_ch4();

            mv_set = (uint32_t)set * 3300UL / 4095UL;
            mv_got = (uint32_t)got * 3300UL / 4095UL;

            uart1_str("DAC_SET=");
            uart1_u32(set);
            uart1_str(" (");
            uart1_u32(mv_set);
            uart1_str("mV)   ADC_READ=");
            uart1_u32(got);
            uart1_str(" (");
            uart1_u32(mv_got);
            uart1_str("mV)\r\n");
        }
        uart1_str("---\r\n");
        HAL_Delay(500);
    }
}
