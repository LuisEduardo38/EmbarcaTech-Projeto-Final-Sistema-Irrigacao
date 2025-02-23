#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/pwm.h"

#include "Bibliotecas/font.h"
#include "Bibliotecas/ssd1306.h"
#include "hardware/i2c.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

const uint8_t led_red_pino = 13;
const uint8_t led_blue_pino = 12;
const uint8_t led_green_pino = 11;

const uint8_t btn_a = 5;
const uint8_t btn_b = 6;
const uint8_t btn_j = 22;

volatile uint32_t ultimo_tempo = 0;
volatile bool segunda_tela = true;
volatile bool limpar_tela = false;

void iniciar_pinos();
void gpio_irq_handler(uint gpio, uint32_t events);

int main(){
    stdio_init_all();
    iniciar_pinos();

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

    bool limite = true;

    uint8_t umidade_solo = 0;
    uint8_t resevatorio_agua = 0;
    uint8_t temperatura_ambiente = 20;

    char umidade [2];
    char resertorio [2];
    char temperatura [2];

    while(true){
        sprintf(umidade, "%d", umidade_solo);
        sprintf(resertorio, "%d", resevatorio_agua);
        sprintf(temperatura, "%d", temperatura_ambiente);
        
        if(limpar_tela == true){
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
            limpar_tela = false;
        }

        if(segunda_tela == true){
            ssd1306_draw_string(&ssd, "SISTEMA DE", 26, 1);
            ssd1306_draw_string(&ssd, "IRRIGACAO", 26, 10);
            ssd1306_draw_string(&ssd, "RESERTORIO", 27, 25);
            ssd1306_draw_string(&ssd, "  :100", 48, 35);
            ssd1306_draw_string(&ssd, resertorio, 38, 35);
            ssd1306_draw_string(&ssd, "UMIDADE SOLO", 14, 45);
            ssd1306_draw_string(&ssd, "  :100", 48, 55);
            ssd1306_draw_string(&ssd, umidade, 38, 55);
        }
        else{
            ssd1306_draw_string(&ssd, "SISTEMA DE", 26, 1);
            ssd1306_draw_string(&ssd, "IRRIGACAO", 26, 10);
            ssd1306_draw_string(&ssd, "TEMPERATURA", 20, 30);
            ssd1306_draw_string(&ssd, "C", 75, 45);
            ssd1306_draw_string(&ssd, temperatura, 55, 45);
        }

        /*if(limite == true){
            umidade_solo++;
            resevatorio_agua++;    
            if(umidade_solo == 100){
                limite = false;
            }
        }
        else if(limite == false){
            umidade_solo--;
            resevatorio_agua--;
            if(umidade_solo == 0){
                limite = true;
            }
        }*/

        if(limite == true){
            temperatura_ambiente++;
            if(temperatura_ambiente == 40){
                limite = false;
            }
        }
        else if(limite == false){
            temperatura_ambiente--;
            if(temperatura_ambiente == 0){
                limite = true;
            }
        }

        pwm_set_gpio_level(led_red_pino, 100 * temperatura_ambiente);
        pwm_set_gpio_level(led_blue_pino, 100 * resevatorio_agua);
        pwm_set_gpio_level(led_green_pino, 100 * umidade_solo);
        ssd1306_send_data(&ssd);

        if((temperatura_ambiente == 9)  && (limite == false)){
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
        }
        
        sleep_ms(500);
    }
}

void iniciar_pinos(){
    gpio_set_function(led_red_pino, GPIO_FUNC_PWM);
    gpio_set_function(led_blue_pino, GPIO_FUNC_PWM);
    gpio_set_function(led_green_pino, GPIO_FUNC_PWM);
    uint slice_numero_blue, slice_numero_green, slice_numero_red;
    slice_numero_blue = pwm_gpio_to_slice_num(led_red_pino);
    slice_numero_blue = pwm_gpio_to_slice_num(led_blue_pino);
    slice_numero_green = pwm_gpio_to_slice_num(led_green_pino);
    pwm_set_clkdiv(slice_numero_red, 4.0);
    pwm_set_clkdiv(slice_numero_blue, 4.0);
    pwm_set_clkdiv(slice_numero_green, 4.0);
    pwm_set_wrap(slice_numero_red, 4000);
    pwm_set_wrap(slice_numero_blue, 10000);
    pwm_set_wrap(slice_numero_green, 10000);
    pwm_set_gpio_level(led_red_pino, 100);
    pwm_set_gpio_level(led_blue_pino, 100);
    pwm_set_gpio_level(led_green_pino, 100);
    pwm_set_enabled(slice_numero_red, true);
    pwm_set_enabled(slice_numero_blue, true);
    pwm_set_enabled(slice_numero_green, true);

    gpio_init(btn_a);
    gpio_init(btn_b);
    gpio_init(btn_j);
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
        }
        else if(gpio == 6){
            printf("BTN_B\n");
            limpar_tela = true;
            segunda_tela = !segunda_tela;
        }
        else if(gpio == 22){
            printf("BTN_J\n");
            reset_usb_boot(0, 0);
        }
    }
}