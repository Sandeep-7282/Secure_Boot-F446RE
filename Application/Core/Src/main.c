#include <main.h>
#include <uart.h>

int main(void){
	uart_init();
	for(int i=0;i<5;i++){
	uart_send_str("-----------Welcome to Application v1.0---------\n");
	}

	while(1);
}
