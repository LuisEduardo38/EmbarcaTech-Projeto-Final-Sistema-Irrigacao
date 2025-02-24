//Bibliotecas para funcionamento em geral do código
#include <stdio.h>
#include <time.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"

//Bibliotecas para funcionamento do display1306
#include "Bibliotecas/font.h"
#include "Bibliotecas/ssd1306.h"
#include "hardware/i2c.h"

//Bibliotecas para funcionamento da matriz de led WS2818B
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "ws2818b.pio.h"

//Parâmetro para configurações do display1306
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

//Declaração das variáveis com os pinos dos leds
const uint8_t led_red_pino = 13;
const uint8_t led_blue_pino = 12;
const uint8_t led_green_pino = 11;

//Declaração das variáveis com os pinos dos botões
const uint8_t btn_a = 5;
const uint8_t btn_b = 6;
const uint8_t btn_j = 22;

//Declaração das variáveis com os pinos da matriz juntamento com a quantidade de leds
const uint8_t matriz_pino = 7;
const uint8_t numero_led = 25;

//Declaração das variáveis globais
volatile uint32_t ultimo_tempo = 0;
volatile bool segunda_tela = true;
volatile bool limpar_tela = false;
volatile bool trava_temperatura = true;
volatile bool trava_tempo = true;

//Declaração dos frames para uso na matriz de leds
int face01 [] = {0, 1, 1, 1, 0,
                 1, 0, 0, 0, 1,
                 0, 0, 0, 0, 0,
                 0, 1, 0, 1, 0,
                 0, 1, 0, 1, 0};

int face02 [] =  {0, 0, 0, 0, 0,
                  1, 1, 1, 1, 1,
                  0, 0, 0, 0, 0,
                  0, 1, 0, 1, 0,
                  0, 1, 0, 1, 0};


int face03 [] =  {1, 0, 0, 0, 1,
                  0, 1, 1, 1, 0,
                  0, 0, 0, 0, 0,
                  0, 1, 0, 1, 0,
                  0, 1, 0, 1, 0};

int face04 [] =  {1, 0, 0, 0, 1,
                  0, 1, 0, 1, 0,
                  0, 0, 1, 0, 0,
                  0, 1, 0, 1, 0,
                  1, 0, 0, 0, 1};

//Protótipos das funções do código
void iniciar_pinos();
uint8_t sensor_temperatura();
void gpio_irq_handler(uint gpio, uint32_t events);
uint8_t sensor_reservatorio_agua(uint8_t reservatorio_atual);
void imprimir_status(uint8_t temperatura, int *numero, PIO pio, int sm);
void manipulacao_matriz(uint8_t temperatura, uint8_t numero, PIO pio, int sm);
void sensor_umidade_solo(uint8_t *umidade_atual, uint8_t *reservatorio_atual, uint8_t temperatura_atual);

//Função principal do código
int main(){
    //Inicializando as bibliotecas
    stdio_init_all();
    iniciar_pinos();
    srand(time(NULL));

    //Declaração das interrupções
    gpio_set_irq_enabled_with_callback(btn_a, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);//Mundar a temperatura
    gpio_set_irq_enabled_with_callback(btn_b, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);//Trocar de tela
    gpio_set_irq_enabled_with_callback(btn_j, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);//Entra no modo bootsel

    //Configurações o display1306
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

    //Configurações da matriz de led WS2818B
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2818b_program);
    ws2818b_program_init(pio, sm, offset, matriz_pino, 800000);

    //Declaração das variáveis para controle dos sensores
    uint8_t umidade_solo = 50;//Variáveis que irá armazenar os dados do sensor de umidade do solo
    uint8_t reservatorio_agua = 100;//Variáveis que irá armazenar os dados do sensor de nível da água
    uint8_t temperatura_ambiente = 0;//Variáveis que irá armazenar os dados do sensor de temperatura ambiente

    //Declaração de string para apresentar os dados dos sensores
    char umidade [3];
    char resertorio [3];
    char temperatura [3];

    //Loop principal
    while(true){
        //IF para permitir a entrada na função sensor_temperatura
        if(trava_temperatura == true){
            temperatura_ambiente = sensor_temperatura();//O dado da temperatura será coletado e armazenado na variável            
            trava_temperatura = false;
        }

        //Chando a função para coletar dado do nível da água e armazenando na variável
        reservatorio_agua = sensor_reservatorio_agua(reservatorio_agua);

        //Chama a função que irá fazer leitura e processar dados do código de umidade do solo e nível da água
        sensor_umidade_solo(&umidade_solo, &reservatorio_agua, temperatura_ambiente);

        //Enviar dados a matriz de led com base na temperatura e nível da água
        manipulacao_matriz(temperatura_ambiente, reservatorio_agua, pio, sm);

        //Converção dos dados das variáveis dos sensores para string para serem exibidos no display
        sprintf(umidade, "%d", umidade_solo);
        sprintf(resertorio, "%d", reservatorio_agua);
        sprintf(temperatura, "%d", temperatura_ambiente);
        
        //IF para limpar a tela antes de troca para a segunda tela com a temperatura
        if(limpar_tela == true){
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
            limpar_tela = false;
        }

        //IF para controlar quais das tela será apresentado ao usuário
        if(segunda_tela == true){//Com a primeira tela sendo para exibir dados de nível da água e umidade do solo
            ssd1306_draw_string(&ssd, "SISTEMA DE", 26, 1);
            ssd1306_draw_string(&ssd, "IRRIGACAO", 26, 10);
            ssd1306_draw_string(&ssd, "RESERTORIO", 27, 25);
            ssd1306_draw_string(&ssd, "  :100", 48, 35);
            ssd1306_draw_string(&ssd, resertorio, 38, 35);
            ssd1306_draw_string(&ssd, "UMIDADE SOLO", 14, 45);
            ssd1306_draw_string(&ssd, "  :100", 48, 55);
            ssd1306_draw_string(&ssd, umidade, 38, 55);
        }
        else{//Com a segunda tela sendo para exibir a temperatura do ambiente
            ssd1306_draw_string(&ssd, "SISTEMA DE", 26, 1);
            ssd1306_draw_string(&ssd, "IRRIGACAO", 26, 10);
            ssd1306_draw_string(&ssd, "TEMPERATURA", 20, 30);
            ssd1306_draw_string(&ssd, "C", 75, 45);
            ssd1306_draw_string(&ssd, temperatura, 55, 45);
        }

        //Alterando o estado do led RGB com base nos dados dos sensores
        pwm_set_gpio_level(led_red_pino, 100 * temperatura_ambiente);//O led red irá aumentar ou diminir a sua intensidade com base na temperatura do ambiente através do PWM
        pwm_set_gpio_level(led_blue_pino, 100 * reservatorio_agua);//O led blue irá aumentar ou diminir a sua intensidade com base no nível da água através do PWM
        pwm_set_gpio_level(led_green_pino, 100 * umidade_solo);//O led green irá aumentar ou diminir a sua intensidade com base na umidade do solo através do PWM
        ssd1306_send_data(&ssd);//Atualiza o display
        
        //Delay do código
        sleep_ms(500);
    }
}

//Função para iniciar os pinos da placa
void iniciar_pinos(){
    //Configuração dos leds para ajuste via PWM
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

    //Configurações dos botões da placa
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

//Função para tratamento de rotinas de interrupções
void gpio_irq_handler(uint gpio, uint32_t events){
    //Pega o tempo atual dos temporizadores da placa
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());
    //Esse IF servi conter o debounce do botões
    if(tempo_atual - ultimo_tempo > 200){
        ultimo_tempo = tempo_atual;
        //IFs para indicar qual botão foi pressionado
        if(gpio == 5){
            trava_temperatura = true;//Trava colocando em true para permite a troca da temperatura
        }
        else if(gpio == 6){//Mudanças em bool para permiti a troca correta das telas
            limpar_tela = true;
            segunda_tela = !segunda_tela;
        }
        else if(gpio == 22){//Entra no modo BootSel
            reset_usb_boot(0, 0);
        }
    }
}

//Função para coletar dados de temperatura
uint8_t sensor_temperatura(){
    uint8_t sensor;

    sensor = 15 + rand() % (40 - 15 + 1);//A temperatura é um valor que é randomizando entre 15 a 40

    return sensor;
}

//Função que irá coletar dados do nível da água como também será responsável por preencher novamente o reservatório
uint8_t sensor_reservatorio_agua(uint8_t reservatorio_atual){
    static bool trava_reservatorio = false;

    //Neste condicional se o nível estiver abaixa de 90% e a trava for FALSE vai entrar
    if((reservatorio_atual < 90) && (trava_reservatorio == false)){
        reservatorio_atual = reservatorio_atual + 5;//Aqui será preenchindo novamente o reservatório
        if(reservatorio_atual > 90){//Aqui se o nível do reservatório passar de 90 a trava será ativada novamente para não encher de mais o reservatório
            trava_reservatorio = true;
        }
    }
    //Nesta outra condicional está vereficando se o nível do reservatório está muito baixo, caso esteja muda a trava para FALSE
    else if((reservatorio_atual < 15) && (trava_reservatorio == true)){
        trava_reservatorio = false;
    }

    //rertornado o valor reservatório_atual
    return reservatorio_atual;
}

//Função para coletar e processar dados de umidade e água
void sensor_umidade_solo(uint8_t *umidade_atual, uint8_t *reservatorio_atual, uint8_t temperatura_atual){
    static bool trava = false;
    //Neste condicional está vereficando se a umidade do solo é inferior a 75 e a trava está FALSE
    if((*umidade_atual < 75) && (trava == false)){
        *umidade_atual = *umidade_atual + 1;//Incrementado mais 1 a umidade do solo
        *reservatorio_atual = *reservatorio_atual - 3;//Para cada uma unidade de umidade de solo irá decrementar menos 3 do reservatório
        if(*umidade_atual >= 75){//Se a umidade for igual ou maior a 75% irá mudar a trava para TRUE
            trava = true;
        }
    }//Caso as condições não sejam atendidas no primeiro IF entrarar neste ELSE
    else{
        //IF para processar quanto será o depreciamento da umidade do solo com base na temperatura
        if(temperatura_atual > 35){
            *umidade_atual = *umidade_atual - 4;
            *reservatorio_atual = *reservatorio_atual - 1;//Para questões de "SIMULAÇÃO" coloquei para diminuir a quantidade de água devido alta temperatura
        }
        else if(temperatura_atual > 30){
            *umidade_atual = *umidade_atual - 3;
        }
        else if(temperatura_atual > 25){
            *umidade_atual = *umidade_atual - 2;
        }
        else{
            *umidade_atual = *umidade_atual - 1;
        }
        
        //IF para mudar a trava caso a umidade caiu para menos que 40%
        if(*umidade_atual < 40){
            trava = false;
        }
    }
}

//Função para manipular a matriz de led com base na temperatura ambiente
void manipulacao_matriz(uint8_t temperatura, uint8_t numero, PIO pio, int sm){
    //Neste IF irá direcionar qual frame será exibido na matriz de led
    if(numero > 80){
        imprimir_status(temperatura, face01, pio, sm);
    }
    else if(numero > 60){
        imprimir_status(temperatura, face02, pio, sm);
    }
    else if(numero > 40){
        imprimir_status(temperatura, face03, pio, sm);
    }
    else if(numero < 30){
        imprimir_status(temperatura, face04, pio, sm);
    }
}

//Função para imprimir os frames na matriz
void imprimir_status(uint8_t temperatura, int *numero, PIO pio, int sm){
    uint8_t red, blue, green;

    //Neste IF irá decider qual será a cor dos leds com base na temperatura
    if(temperatura >= 30){
        red = 100;
        blue = 0;
        green = 0;
    }
    else if(temperatura >= 25){
        red = 100;
        blue = 100;
        green = 0;
    }
    else if(temperatura >= 20){
        red = 100;
        blue = 0;
        green = 100;
    }
    else if(temperatura >= 15){
        red = 0;
        blue = 100;
        green = 0;
    }

    //For para percorrer todos os leds da matriz para ativar ou desativar os leds espessificos
    for(uint8_t i = 0; i < numero_led; i++){
        uint8_t RED = (uint8_t)(red * numero[i]);
        uint8_t BLUE = (uint8_t)(blue * numero[i]);
        uint8_t GREEN = (uint8_t)(green * numero[i]);

        pio_sm_put_blocking(pio, sm, GREEN);
        pio_sm_put_blocking(pio, sm, RED);
        pio_sm_put_blocking(pio, sm, BLUE);
    }
}