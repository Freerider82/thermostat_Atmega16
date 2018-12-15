
#include "project_inkub.h"
#include "driver_nokia5110.h"
#include "lcd_nokia5110pin.h"
#include "Font_6x8.h"

//****************************************************************************************************************
void initLCD(){
	
	DDR_LCD	|= (1<<SCK) | (1<<SDI) | (1<<D_C) | (1<<_SCE) | (1<<_RES);
	SetBit(PORT_LCD,_RES); // Set _RES HIGH.
	SetBit(PORT_LCD,_SCE); // Disable Chip.
	ClearBit(PORT_LCD,_RES); // Reset the LCD.
	
	_delay_ms(100);    // Wait 100ms.
	SetBit(PORT_LCD,_RES);        // Awake LCD from RESET state.
	#ifdef NOKIA_5110
	writecom(0x21);        // Activate Chip and H=1.
	writecom(0x13);	//Установить схему смещения напряжения, команда
	writecom(0x04);	//Установить режим температурной коррекции, команда
	writecom(0xB8);	// Set LCD Voltage to about 6.42V.
	writecom(0x20);	//Вернуться в стандартный набор команд, послав
	#endif
	
	#ifdef NOKIA_3310
	writecom(0x21);        // Activate Chip and H=1.
	writecom(0xC2);        // Set LCD Voltage to about 7V.
	writecom(0x13);        // Adjust voltage bias.
	writecom(0x20);        // Horizontal addressing and H=0.
	writecom(0x09);        // Activate all segments.
	writecom(0x08);        // Blank the Display.
	
	#endif
	
	writecom(0x0C);        // Display Normal.
	clearram();
	cursorxy(0,0);        // Cursor Home.
}
//****************************************************************************************************************
void writecom(uint8_t command_in){
	
	ClearBit(PORT_LCD,D_C);// Select Command register.
	ClearBit(PORT_LCD,_SCE);// Select Chip.
	clockdata(command_in);    // Clock in command bits.
	SetBit(PORT_LCD,_SCE); // Disable Chip.

}
//****************************************************************************************************************
void clockdata(uint8_t bits_in){
	
	uint8_t bitcnt;
	
	for (bitcnt=8; bitcnt>0; bitcnt--)
	{
		ClearBit(PORT_LCD,SCK);// Set Clock Idle level LOW.
		if ((bits_in&0x80)==0x80) {SetBit(PORT_LCD,SDI);}        // PCD8544 clocks in the MSb first.
		else {ClearBit(PORT_LCD,SDI);}
		SetBit(PORT_LCD,SCK);// Data is clocked on the rising edge of SCK.
		bits_in=bits_in<<1;                        // Logical shift data by 1 bit left.
		
	}
	
}
//****************************************************************************************************************
void clearram(void){
	uint16_t ddram;
	
	cursorxy(0,0);                                            // Cursor Home.
	for (ddram=910;ddram>0;ddram--)    {
	writedata(0x00);}        // 6*84 = 504 DDRAM addresses.
	cursorxy(0,0);
}
//****************************************************************************************************************
void cursorxy(uint8_t x,uint8_t  y){
	writecom(0x40|(y&0x07));    // Y axis
	writecom(0x80|(x&0x7f));    // X axis
}
//****************************************************************************************************************
void writedata(uint8_t data_in){
	
	SetBit(PORT_LCD,D_C); // Select Data register.
	ClearBit(PORT_LCD,_SCE);// Select Chip.
	clockdata(data_in);        // Clock in data bits.
	SetBit(PORT_LCD,_SCE);// Deselect Chip.
	
}
/****************************************************************************/
void contrastLCD(uint8_t contrast) {

	//  LCD Extended Commands.
	writecom(0x21);

	// Set LCD Vop (Contrast).
	writecom(0x80 | contrast);

	//  LCD Standard Commands, horizontal addressing mode.
	writecom(0x20);
}
//****************************************************************************************************************
void lcd_put_char(uint8_t symbol,uint8_t flag){
	//ptr указатель на строку символа symbol

	uint8_t *ptr=table+symbol;
	uint8_t temp;
	
	for(uint8_t i=0;i<5;i++){
		
		temp=(flag==INVERSION)? ~pgm_read_byte(ptr+i):pgm_read_byte(ptr+i);
		//Передаем 5 байт этого символа
		
		writedata(temp);
		writedata(temp);
		
	}
	//_delay_ms(250);
	temp=(flag==INVERSION)? 0xFF:0;
	writedata(temp);
	writedata(temp);
}
//****************************************************************************************************************
void printBigNumber(uint8_t cursor_x,uint8_t cursor_y,uint16_t number,uint8_t flagPoint){
	uint8_t *ptr[3];
	uint8_t _integer=INTEGER(number);
	int8_t counter=0;
	
	for(;counter<3;counter++){
		ptr[counter]=table_big_numbers+_integer%10;
		_integer/=10;
		if(_integer==0) break;
	}
	cursorxy(cursor_x,cursor_y);
	for(;counter>=0;counter--){
		for(uint8_t i=0;i<20;i++){
			writedata(pgm_read_byte(ptr[counter]+i));
			if(i==9){cursorxy(cursor_x,cursor_y+9);};
		}
		writedata(0);
		 
		cursorxy(cursor_x+=11,cursor_y);
	}
	if(flagPoint==1){
		cursorxy(cursor_x,cursor_y+9);
		lcd_put_char(BIG_POINT,NOT_INVERSION);
	}
	cursorxy(cursor_x+=6,cursor_y);
	
	
	ptr[0]=table_big_numbers+FRACTION(number);
	for(uint8_t i=0;i<20;i++){
		writedata(pgm_read_byte(ptr[0]+i));
		if(i==9){cursorxy(cursor_x,cursor_y+9);};
	}
	writedata(0);
	//_delay_ms(250);
}
//****************************************************************************************************************
void lcd_print(uint8_t *str,uint8_t flag){
	while(*str!=0){
		lcd_put_char(*str,flag);
		str++;
	}
}

