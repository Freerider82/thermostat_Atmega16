#include "project_inkub.h"

void initUART(void) {
	
	UCSRB=(0<<RXCIE) | (0<<TXCIE) | (0<<UDRIE) | (0<<RXEN) | (1<<TXEN) | (0<<UCSZ2) | (0<<RXB8) | (0<<TXB8);
	UCSRC=(1<<URSEL) | (0<<UMSEL) | (0<<UPM1) | (0<<UPM0) | (0<<USBS) | (1<<UCSZ1) | (1<<UCSZ0) | (0<<UCPOL);
	
	//(2битUCSRB)UCSZ2=0 (2-1бит) UCSZ1-UCSZ0=1 011 8 битная передача (исходя из таблицы)
	UBRRH=0x00;
	UBRRL=25;//51;//
}
void sendDataUART(uint8_t *data,uint8_t size){
	
	for (uint8_t i=0;i<size;i++)	{
		while(BitIsClear(UCSRA,UDRE));
		UDR=*data;
		data++;
	}
}
static int uart_putchar(char c, FILE *stream){

	if (c == '\n')
	uart_putchar('\r', stream);
	while(BitIsClear(UCSRA, UDRE));
	UDR = c;
	return 0;
}