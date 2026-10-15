/*
 * 节点 1 · 点亮 LED 灯（HAL 库版 · 练习模板）
 *
 * practice 分支 = 你手敲的地方；main 分支 = 参考答案（卡壳15分钟以上再看）。
 * 目标现象：LED1（PD10，高电平亮）500ms 亮 / 500ms 灭循环。
 *
 * 脚手架已给（不用动）：HAL_Init、SysTick 心跳（stm32f4xx_it.c）、MX_GPIO_Init 空壳。
 * 你要写的：①MX_GPIO_Init 里的三小步 ②主循环的亮灭交替。
 * 教材：docs/教程-节点1-点亮LED灯.md 第四节。
 */

#include "stm32f4xx_hal.h"

static void MX_GPIO_Init(void);

int main(void)
{
    HAL_Init();        /* 脚手架：SysTick 1ms 心跳，HAL_Delay 靠它 */
    MX_GPIO_Init();    /* ← 你来填 */

    while (1)
    {
        /* ⑤ 亮 500ms → 灭 500ms → 循环
           提示：置高/置低用哪个函数？延时用哪个函数？（教程 4.1 对照表） */
    }
}

static void MX_GPIO_Init(void)
{
    /* ① 先声明"参数单"变量：类型叫 GPIO_InitTypeDef（教程 4.2 有完整写法） */

    /* ② 开 GPIOD 端口时钟（HAL 宏，教程 4.1 对照表第 1 行） */

    /* ③ 先给安全初态：配模式前先把 PD10 拉低（灭）——和寄存器版"先给初态"同一个道理 */

    /* ④ 填四字段再交给 HAL_GPIO_Init：
       Pin   = 第 10 脚
       Mode  = 推挽输出
       Pull  = 不上拉不下拉
       Speed = 低速
       然后：谁替你写 MODER/OTYPER/OSPEEDR/PUPDR？（教程 4.1） */
}
