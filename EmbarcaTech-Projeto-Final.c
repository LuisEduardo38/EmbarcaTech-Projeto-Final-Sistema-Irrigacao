#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "Bibliotecas/font.h"
#include "Bibliotecas/ssd1306.h"
#include "hardware/i2c.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

const uint8_t led_red_pinos = 13;
const uint8_t led_blue_pinos = 12;
const uint8_t led_green_pinos = 11;
volatile bool estado_red = false;
volatile bool estado_blue = false;
volatile bool estado_green = false;

const uint8_t btn_a = 5;
const uint8_t btn_b = 6;
const uint8_t btn_j = 22;

volatile uint32_t ultimo_tempo = 0;

void iniciar_pinos();
void gpio_irq_handler(uint gpio, uint32_t events);

int main(){
    stdio_init_all();
    iniciar_pinos();

    uint16_t umidade_solo = 500;
    uint16_t umidade_ar = 500;
    uint16_t resevadorio_agua = 1000;

    gpio_set_irq_enabled_with_callback(btn_a, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(btn_b, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(btn_j, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    while(true){

        ssd1306_draw_string(&ssd, "SISTEMA DE", 26, 1);
        ssd1306_draw_string(&ssd, "IRRIGACAO", 26, 10);
        ssd1306_send_data(&ssd);
        sleep_ms(1000);
    }
}

void iniciar_pinos(){
    gpio_init(led_red_pinos);
    gpio_init(led_blue_pinos);
    gpio_init(led_green_pinos);
    gpio_set_dir(btn_a, GPIO_IN);
    gpio_set_dir(btn_b, GPIO_IN);
    gpio_set_dir(btn_j, GPIO_IN);
    gpio_pull_up(btn_a);
    gpio_pull_up(btn_b);
    gpio_pull_up(btn_j);
}

void gpio_irq_handler(uint gpio, uint32_t events){
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());
    if(tempo_atual - ultimo_tempo > 200){
        ultimo_tempo = tempo_atual;
        if(gpio == 5){
            printf("BTN_A\n");
            reset_usb_boot(0, 0);
        }
        else if(gpio == 6){
            printf("BTN_B\n");
        }
        else if(gpio == 22){
            printf("BTN_J\n");
        }
    }
}