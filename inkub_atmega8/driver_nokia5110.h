#ifndef lcd5110_def
#define lcd5110_def

#define INTEGER(x) (x)/10
#define FRACTION(x) (x)%10
#define INVERSION		1
#define NOT_INVERSION	0
#define NO_POINT		0
#define POINT			1

void initLCD(void);
void writecom(uint8_t);//Отправка команды
void clockdata(uint8_t);//Программный SPI c отправкой байта
void clearram(void);//Очистка экрана
void cursorxy(uint8_t ,uint8_t );//Установка курсора	
void writedata(uint8_t);//Отправка данных
void contrastLCD(uint8_t);//Устанавливает контраст дисплея

void lcd_put_char(uint8_t,uint8_t );
void printBigNumber(uint8_t, uint8_t, uint16_t ,uint8_t);
static int lcd_putchar(uint8_t ,FILE *);
void lcd_print(uint8_t *,uint8_t);

static int lcd_putchar(uint8_t c,FILE *stream){
	
	if (c == '\n')
	lcd_putchar('\r', stream);
	lcd_put_char(c,NOT_INVERSION);
	return 0;
	
}
#endif