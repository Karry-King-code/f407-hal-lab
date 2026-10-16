/*
 * 节点 2 · 按键控制 LED（HAL 库版 · 练习模板）
 *
 * 硬件事实与寄存器版相同：KEY1 = PD13（低有效，板载上拉），LED1 = PD10（高电平亮）。
 * 目标行为（你寄存器版写的同款）：按一下 K1 → 红灯开始 500ms 闪；再按一下 → 停止灭。
 *
 * 脚手架已给：主循环的分片节拍逻辑（你刚在寄存器版吃透的那套，一句没改）。
 * 你要写的两处：
 *   A. MX_GPIO_Init：开时钟宏 + PD13 输入四字段 + PD10 输出四字段 + 上电先灭
 *   B. key_pressed：读按键 = HAL_GPIO_ReadPin（总文档节点二章有提示）
 */

#include "stm32f4xx_hal.h"

static void MX_GPIO_Init(void);
uint8_t key_pressed(void);

int main(void)
{
  uint8_t  running = 0;
  uint16_t slice   = 0;

  HAL_Init();
  MX_GPIO_Init();                     /* ← 你来填（A） */

  while (1)
  {
    if (key_pressed())                /* ← 你来填（B） */
    {
      running = !running;             /* 开 <-> 停 */
      if (running == 0)
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, GPIO_PIN_RESET);   /* 停止：灯灭 */
    }
    if (running)
    {
      if (++slice >= 50)              /* 50 片 x 10ms = 500ms */
      {
        slice = 0;
        HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_10);   /* HAL 自带翻转（= ODR ^= 那行的代办员） */
      }
    }
    HAL_Delay(10);                    /* 节拍器：每圈 10ms */
  }
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};   /* 两颗脚可以共用一个"参数单" */

  /* ① 开 GPIOD 时钟（HAL 宏） */

  /* ② PD13 输入：
     Pin=13号脚 / Mode=GPIO_MODE_INPUT / Pull=?（板上已有外部上拉，想想填哪个）/ Speed 低速
     然后 HAL_GPIO_Init */

  /* ③ PD10 推挽输出：四字段 + HAL_GPIO_Init（节点一的代码）
     上电先灭：HAL_GPIO_WritePin(...RESET) */
}

uint8_t key_pressed(void)
{
  /* 思路同寄存器版：边沿检测 + 消抖 + 等松手
     把寄存器版里的 (GPIOD->IDR & GPIO_IDR_ID13) 换成 HAL 的读引脚函数：
     HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_13) —— 返回 GPIO_PIN_RESET(=0 按下) / GPIO_PIN_SET(=1 松开)
     delay 用 HAL_Delay */
  return 0;   /* ← 写完记得让这函数真的能返回 1 */
}
