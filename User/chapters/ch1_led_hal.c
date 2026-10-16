/*
 * 节点 1 · 点亮 LED 灯（HAL 库版 · 练习）
 *
 * 硬件事实与寄存器版相同：LED1 = PD10，高电平亮，500ms 节奏，上电先灭。
 * 时钟：不写 SystemClock_Config，复位默认 HSI 16MHz（HAL_Init 已把 SysTick 配成 1ms 心跳）。
 */

#include "stm32f4xx_hal.h"

int main(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  HAL_Init();                     /* HAL 自我准备：SysTick 1ms 心跳、NVIC 分组 */

  __HAL_RCC_GPIOD_CLK_ENABLE();   /* 开 GPIOD 时钟（HAL 替你写 AHB1ENR 的 bit3） */

  GPIO_InitStruct.Pin   = GPIO_PIN_10;          /* PD10 */
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;  /* 推挽输出（写 MODER=01、OTYPER=0） */
  GPIO_InitStruct.Pull  = GPIO_NOPULL;          /* 不上下拉（PUPDR=00） */
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;  /* 低速（OSPEEDR=00） */
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);       /* 一句替你四连写：MODER/OTYPER/OSPEEDR/PUPDR */

  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, GPIO_PIN_RESET);   /* 上电先灭（BSRR 高 16 位清零） */

  while (1)
  {
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, GPIO_PIN_RESET);  /* 灭：BR10 = 输出低电平 */
    HAL_Delay(500);                                         /* HAL 毫秒延时（uwTick 差值法） */
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, GPIO_PIN_SET);    /* 亮：BS10 = 输出高电平 */
    HAL_Delay(500);
  }
}
