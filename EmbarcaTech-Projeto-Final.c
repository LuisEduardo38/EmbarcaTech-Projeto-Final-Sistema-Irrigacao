#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"

int main(){
    stdio_init_all();

    while(true){
        printf("\n");
        sleep_ms(1000);
    }
}