/*
 * 节点 7 · LVGL 移植（HAL 库版）
 *
 * 硬件：ST7789 240x320（SCK=PB3 MOSI=PB5 DC=PB0 CS=PA15 RST=PC7 LED=PB1）
 * LVGL：v8.3.11 / 16 位色 / 大端（LV_COLOR_16_SWAP=1）
 *
 * 与寄存器版的三处差别：
 *   ① SPI 用 HAL_SPI_Transmit 批量发（寄存器版逐字节）
 *   ② 延时用 HAL_Delay（HAL_Init 的 SysTick 心跳）
 *   ③ GPIO 用 HAL_GPIO_WritePin
 * 移植三要素完全相同：lv_init → 注册显示(flush_cb) → 循环 tick + timer_handler
 */

#include "stm32f4xx_hal.h"
#include "lvgl.h"

#define CS_L()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET)
#define CS_H()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET)
#define DC_L()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET)
#define DC_H()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET)
#define RST_L()  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET)
#define RST_H()  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET)
#define LED_ON() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET)

#define LCD_W 240
#define LCD_H 320

SPI_HandleTypeDef hspi1;

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();

    gpio.Pin       = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin   = GPIO_PIN_0 | GPIO_PIN_1;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin  = GPIO_PIN_15;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin  = GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOC, &gpio);

    CS_H(); DC_H();
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
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
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

static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t b[4];
    lcd_cmd(0x2A);
    b[0] = x0 >> 8; b[1] = x0 & 0xFF; b[2] = x1 >> 8; b[3] = x1 & 0xFF;
    CS_L(); DC_H(); HAL_SPI_Transmit(&hspi1, b, 4, 100); CS_H();

    lcd_cmd(0x2B);
    b[0] = y0 >> 8; b[1] = y0 & 0xFF; b[2] = y1 >> 8; b[3] = y1 & 0xFF;
    CS_L(); DC_H(); HAL_SPI_Transmit(&hspi1, b, 4, 100); CS_H();

    lcd_cmd(0x2C);
}

static void lcd_init(void)
{
    RST_H(); HAL_Delay(10);
    RST_L(); HAL_Delay(20);
    RST_H(); HAL_Delay(120);

    lcd_cmd(0x01); HAL_Delay(150);
    lcd_cmd(0x11); HAL_Delay(120);
    lcd_cmd(0x3A); lcd_data(0x55);
    lcd_cmd(0x36); lcd_data(0x00);      /* 竖屏；想横屏改 0x60 并对调宽高 */
    lcd_cmd(0x20);                      /* 反色关（节点六实测） */
    lcd_cmd(0x13);
    lcd_cmd(0x29); HAL_Delay(20);
}

/* ============ LVGL 移植 ============ */
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[LCD_W * 20];
static lv_disp_drv_t disp_drv;

static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t n = (uint32_t)(area->x2 - area->x1 + 1) * (uint32_t)(area->y2 - area->y1 + 1);

    lcd_set_window((uint16_t)area->x1, (uint16_t)area->y1,
                   (uint16_t)area->x2, (uint16_t)area->y2);
    CS_L(); DC_H();
    HAL_SPI_Transmit(&hspi1, (uint8_t *)color_p, (uint16_t)(n * 2), 500);  /* 整块一次发完 */
    CS_H();

    lv_disp_flush_ready(drv);
}

static void ui_create(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x102040), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "F407 + LVGL 8.3");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *sub = lv_label_create(scr);
    lv_label_set_text(sub, "ST7789 240x320");
    lv_obj_set_style_text_color(sub, lv_color_hex(0x80C0FF), 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 55);

    lv_obj_t *bar = lv_bar_create(scr);
    lv_obj_set_size(bar, 180, 18);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, 40);
    lv_bar_set_value(bar, 65, LV_ANIM_OFF);
}

int main(void)
{
    HAL_Init();
    MX_SPI1_Init();
    LED_ON();
    lcd_init();

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, LCD_W * 20);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = LCD_W;
    disp_drv.ver_res  = LCD_H;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    ui_create();

    while (1)
    {
        lv_tick_inc(5);
        lv_timer_handler();
        HAL_Delay(5);
    }
}
