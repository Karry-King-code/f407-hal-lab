/*
 * 节点 1 · 点亮 LED 灯（HAL 库版 · 参考答案）
 *
 * 硬件事实与寄存器版完全相同（见 docs/硬件事实表.md）：
 *   LED1 = PD10，高电平点亮，500ms 闪烁
 *
 * 时钟：不调用 SystemClock_Config，保持复位默认 HSI 16MHz
 *（与寄存器版同基准；HAL_Init 已把 SysTick 配成 1ms 中断，HAL_Delay 可直接用）
 */

#include "stm32f4xx_hal.h"

static void MX_GPIO_Init(void);

int main(void)
{
    HAL_Init();                 /* HAL 自我准备：SysTick 1ms 心跳、NVIC 分组 */
    MX_GPIO_Init();             /* 我们的活：把 PD10 配成输出 */

    while (1)
    {
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, GPIO_PIN_SET);     /* 亮 */
        HAL_Delay(500);
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, GPIO_PIN_RESET);   /* 灭 */
        HAL_Delay(500);
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();   /* 开 GPIOD 时钟（HAL 替你写 AHB1ENR.bit3） */

    /* 先给安全初态（灭），再配模式——顺序和寄存器版一致的道理 */
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin   = GPIO_PIN_10;        /* 第 10 脚 */
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP; /* 推挽输出 */
    GPIO_InitStruct.Pull  = GPIO_NOPULL;         /* 不上不下 */
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW; /* 低速够用 */
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);      /* HAL 替你写 MODER/OTYPER/OSPEEDR/PUPDR */
}
