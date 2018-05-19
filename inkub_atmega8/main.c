/*
* inkub_atmega8.c
*
* Created: 06.04.2018 14:13:56
* Author : Oleg
*/

#include "project_inkub.h"
#include "module_ds18b20.h"
#include "myUart.h"
#include <avr/interrupt.h>

#define  _DEBUG



void send_SoundTextMessage( uint8_t );
uint8_t controlPower(uint16_t,uint16_t);
uint8_t checkChange(uint8_t);
void settingPreferences(uint8_t);
void init_mc(void);

uint16_t e_tempHigh EEMEM = T_HIGH_START;
uint8_t  e_valuePWM EEMEM = OCR1A_START;
uint8_t  e_deltaRunOutTemp EEMEM = DELTA_RUN_OUT_TEMP_START;
uint8_t  e_manualOrAuto EEMEM = 0;


uint16_t selectedItem=1;
uint8_t itemMenu=0;
uint16_t  *ptrInISR;

uint16_t counter=0;
uint8_t regAB=0;

uint8_t statePort=0;
uint16_t tempHigh;//Верхний предел
uint16_t delta_runOutTemp; //Выбег температуры
uint16_t limitHigh=0;
uint16_t manualOrAuto=0;

union bitByte{
	struct {
		unsigned ValueHighIsChange:1;
		unsigned RunOutIsCalculate:1;
		unsigned Power:1;
		unsigned Settings:1;
		unsigned ButtonIsPressed:1;
		unsigned ButtonIsPressedInISR:1;
	}ts; //ts сокращенное this-этот
	uint8_t byte;
}flag;



int main(void)
{
	//Используем printf для вывода
	static FILE mystdout = FDEV_SETUP_STREAM(lcd_putchar, NULL,
	_FDEV_SETUP_WRITE);
	stdout=&mystdout;
	
	
	
	OWI_device allDevices[MAX_DEVICES];
	
	uint16_t temperature[2] = {0,0};
	
	uint16_t averageTemperature;
	uint8_t iDevices = 0;
	uint8_t crcFlag = 0;
	
	uint16_t run_out_temperature=0;//Выбег температуры из-за инертности нагревателя
	
	uint16_t valueHigh=0;
	uint8_t count=0;
	uint16_t lastValue;
	uint8_t delta=0;
	
	flag.byte=0; //Все флаги в 0
	
	init_mc();
	//Чтение из памяти EEPROM начальных или установленных значений
	tempHigh=eeprom_read_word(&e_tempHigh);	
	
	if(tempHigh==0xFFFF){
		eeprom_write_word(&e_tempHigh,T_HIGH_START);
		eeprom_write_byte(&e_valuePWM,OCR1A_START);
		eeprom_write_byte(&e_deltaRunOutTemp,DELTA_RUN_OUT_TEMP_START);		
		eeprom_write_byte(&e_manualOrAuto,0);
	}
	
	tempHigh=eeprom_read_word(&e_tempHigh);
	delta_runOutTemp=eeprom_read_byte(&e_deltaRunOutTemp);
	OCR1A=eeprom_read_byte(&e_valuePWM);
	manualOrAuto=eeprom_read_byte(&e_manualOrAuto);
	
	//Определение  кол-ва датчиков, запись их адрессов в allDevices
	crcFlag = OWI_SearchDevices(allDevices, MAX_DEVICES, BUS, &iDevices);
	if(iDevices!=MAX_DEVICES) crcFlag=FEWER_DEVICES;
	send_SoundTextMessage(crcFlag);
	cursorxy(0,1);
	printf("К-во дат-ков %d",iDevices);
	
	//Настройка датчиков на опр.разрешение
	for(uint8_t i=0;i<MAX_DEVICES;i++){
		while(DS18B20_WriteScratchpad(BUS, allDevices[i].id, 0, 0, DS18B20_12BIT_RES)!=WRITE_SUCCESSFUL);
	}
	
	while (1){
		if (flag.ts.Settings==0)
		{
			//Чтение температуры 2 датчиков
			for(uint8_t i=0;i<MAX_DEVICES;){
				crcFlag = DS18B20_ReadTemperature(BUS, allDevices[i].id, &temperature[i]);
				if (crcFlag != READ_SUCCESSFUL){
					send_SoundTextMessage(crcFlag);
					cursorxy(0,1);
					printf("Датчик %d",i+1);
					ClearBit(DDR_POWER,PIN_POWER);
				}
				else{
					i++;
				}
			}
			
			averageTemperature=(temperature[0]+temperature[1])/2;
			
			
			//
			flag.ts.Power=controlPower(averageTemperature,valueHigh);
			/*Управление нагрузкой в автоматическом режиме */
			{
				//flag.ts.ValueHighIsChange флаг 0- valueHigh не изменилось,
				// 1- изменилось стало больше на 1 (SHORT_LIMIT_TEMP) градус или равным tempHigh
				// Сброс ValueHighIsChange при измерении выбега температуры
				if(flag.ts.ValueHighIsChange==0){
					
					if(averageTemperature<=tempHigh-SHORT_LIMIT_TEMP){
						valueHigh = averageTemperature+SHORT_LIMIT_TEMP;
						delta = DELTA_PWM;
					}
					else{
						valueHigh = tempHigh;
						delta = 1;
					}
					flag.ts.ValueHighIsChange=1;
				}
				
				
				if(flag.ts.Power==POWER_ON){
					flag.ts.RunOutIsCalculate=0;
					run_out_temperature=valueHigh;
					//Счетчик для проверки стабильности температуры
					if(count==0){
						lastValue=averageTemperature;
					}
					count++;
					if(count==10){
						count=0;
						//Если температура не меняется и верхний предел != установленому в настройках пределу
						if((lastValue==averageTemperature)&&(valueHigh!=tempHigh)){
							//Увеличиваем мощность если тек темп-ра = прошлой
							//и пока не достигли верхнего предела
							OCR1A+=((OCR1A<255)&&(manualOrAuto&1==1))? 1:0;
						}
					}
					
					//Если температура начала падать то увеличиваем мощность
					if(averageTemperature < valueHigh-2*SHORT_LIMIT_TEMP){
						OCR1A+=((OCR1A<256-DELTA_PWM)&&(manualOrAuto&1==1))? DELTA_PWM:0;
						flag.ts.ValueHighIsChange=0;
						send_SoundTextMessage(TEMP_IS_DOWN);
					}
				}
				else{
					//flag.ts.Power==POWER_OFF
					//При выключенном измеряем выбег температуры run_out_temperature
					if(averageTemperature>=run_out_temperature){
						run_out_temperature=averageTemperature;
					}
					else if(flag.ts.RunOutIsCalculate==0){
						flag.ts.RunOutIsCalculate=1;
						flag.ts.ValueHighIsChange=0;
						if(run_out_temperature>valueHigh+delta_runOutTemp){
							OCR1A-=((OCR1A>delta)&&(manualOrAuto&1==1))? delta:0;//delta это насколько изменится ШИМ
						}
					}
				}
			}
			
			clearram();
			printf("%d.%d",INTEGER(temperature[0]),FRACTION(temperature[0]));
			cursorxy(30,0);
			printf("%d.%d",INTEGER(temperature[1]),FRACTION(temperature[1]));
			cursorxy(0,1);
			printf("H %d L %d",valueHigh,valueHigh-2*SHORT_LIMIT_TEMP);
			cursorxy(0,2);
			printf("Верх %d.%d",INTEGER(tempHigh),FRACTION(tempHigh));
			cursorxy(0,3);
			printf("Rt %d dT %d",run_out_temperature,run_out_temperature-valueHigh );
			cursorxy(0,4);
			printf("PWM %d Fl %d",OCR1A,flag.ts.ValueHighIsChange);
			cursorxy(0,5);
			printf("Средняя %d.%d",INTEGER(averageTemperature),FRACTION(averageTemperature));
			
			//Если кнопка энкодера не нажата
			if(BitIsSet(PIN_ENCODER,PIN_BIT_ENCODER_BUTTON)) flag.ts.ButtonIsPressed=0;
			
			//Если кнопка энкодера нажата переходим в режим настроек
			if((BitIsClear(PIN_ENCODER,PIN_BIT_ENCODER_BUTTON))&&(flag.ts.ButtonIsPressed==0)){
				ClearBit(DDR_POWER,PIN_POWER);
				itemMenu=0;
				flag.ts.Settings=1;
				flag.ts.ButtonIsPressedInISR=0;
				flag.ts.ButtonIsPressed=1;
				limitHigh=4;
				selectedItem=4;
				ptrInISR=&selectedItem;
				clearram();
				settingPreferences(selectedItem);
				sei();
			}
		}
		else{
			//flag.ts.Settings=1
			//Опрос энкодера и кнопки
			while(1){
				//Если прокрутили энкодер или нажали на кнопку
				if((checkChange(*ptrInISR)!=0)||(flag.ts.ButtonIsPressedInISR!=0)){
					//Если ptrInISR изменилось на отрицательное число
					if((*ptrInISR&0x8000)==0x8000 )	*ptrInISR=limitHigh;
					
					if(*ptrInISR>limitHigh) *ptrInISR=0;
					
					settingPreferences(selectedItem);
					
					//cursorxy(0,5);
					//printf("item %x fl %d",*ptrInISR,flag.ts.Settings );
					break;
				}
			}
		}
	}
}

/*****************************************************************************/
void send_SoundTextMessage( uint8_t code_Message){
	
	clearram();
	switch(code_Message){
		
		case SEARCH_SUCCESSFUL:
		printf("ВСЕ ОК");
		break;
		
		case READ_CRC_ERROR:
		cursorxy(0,1);
		printf("ОШИБКА ЧТЕНИЯ ");
		break;
		
		case FEWER_DEVICES:
		printf("Датчиков мало");
		break;
		
		case READ_NEGATIVE:
		printf("Отриц темп-ра");
		break;
		
		case SEARCH_CRC_ERROR:
		printf("ОШ ПОИСКА CRC");
		break;
		
		case SEARCH_ERROR:
		printf("НЕТ ДАТЧИКА");
		break;
		
		case TEMP_IS_DOWN:
		printf("Температура   падает");
		break;
		
		case TEMP_IS_STABLE:
		printf("Температура   стабильная");
		break;
	}
	
}
/*****************************************************************************/
uint8_t controlPower(uint16_t value,uint16_t high){
	
	if(value>high){
		ClearBit(DDR_POWER,PIN_POWER);
		return POWER_OFF;
	}
	else {
		SetBit(DDR_POWER,PIN_POWER);
		return POWER_ON;
	}
}
/*****************************************************************************/
uint8_t checkChange(uint8_t new){
	static uint8_t last = 0;
	uint8_t temp=0;
	if(new==last) return 0;
	else {
		temp = (new>last)? -1:1;
		last=new;
	}
	return temp;
}
/*****************************************************************************/
void settingPreferences(uint8_t item){
	uint8_t *string[]={" Предел t "," Мощность "," Выбег t "," Руч/Авто "," Выход "};
	
	uint8_t flagPrint=NOT_INVERSION;
	if(flag.ts.ButtonIsPressedInISR==1){
		if(item<4){
			clearram();
			cursorxy(0,0);
			printf(string[item]);
			
			if(itemMenu==0){
				itemMenu=item+1;
				
				switch(item){
					case 0: limitHigh=1270;ptrInISR=&tempHigh;break;
					case 1: limitHigh=254;ptrInISR=&OCR1A;break;
					case 2: limitHigh=100;ptrInISR=&delta_runOutTemp;break;
					case 3: limitHigh=1;ptrInISR=&manualOrAuto;break;
				}
				}else{
				limitHigh=4;
				ptrInISR=&selectedItem;
				itemMenu=0;
			}
		}
		else{
			//flag.ts.ButtonIsPressedInISR=0 выходим из режима настроек
			cli();
			flag.ts.Settings=0;
			eeprom_update_word(&e_tempHigh,tempHigh);
			eeprom_update_byte(&e_valuePWM,OCR1A);
			eeprom_update_byte(&e_deltaRunOutTemp,delta_runOutTemp);
			eeprom_update_byte(&e_manualOrAuto,manualOrAuto);
		}
		flag.ts.ButtonIsPressedInISR=0;
	}
	switch(itemMenu){
		case 0:
		clearram();
		for (uint8_t y=0;y<5;y++){
			cursorxy(0,y);
			flagPrint=(item==y)? INVERSION:NOT_INVERSION;
			lcd_print(string[y],flagPrint);
			switch(y){
				case 0: printf("%d.%d",INTEGER(tempHigh),FRACTION(tempHigh));break;
				case 1: printf("%d%c",PERCENT_PWM(OCR1A),'%');break;
				case 2: printf("%d.%d",INTEGER(delta_runOutTemp),FRACTION(delta_runOutTemp));break;
				case 3: ((manualOrAuto&1)==0)? printf("Ручн"):printf("Авто");break;
			}
		}
		break;
		case 1:	printBigNumber(30,2,tempHigh,POINT);break;
		case 2:	printBigNumber(30,2,PERCENT_PWM(OCR1A),NO_POINT);break;
		case 3:	printBigNumber(30,2,delta_runOutTemp,POINT);break;
		case 4:
		
		cursorxy(30,2);
		((manualOrAuto&1)==0)? printf(" Ручной"):printf("Автомат");
		break;
		
	}
}
/*****************************************************************************/
void init_mc(){
	
	#ifdef _DEBUG
	DDRB=1<<DDB1 ;
	#endif
	
	TCCR0=(0<<WGM00) | (0<<COM01) | (0<<COM00) | (0<<WGM01) | (1<<CS02) | (0<<CS01) | (0<<CS00);
	TIMSK=1<<OCIE0;
	OCR0=8;
	
	// Timer/Counter 1 initialization
	// Clock source: System Clock
	// Clock value: 1,953 kHz
	// Mode: Fast PWM top=0x00FF
	// OC1A output: Non-Inverted PWM
	// Timer Period: 0,13107 s
	// Output Pulse(s):
	// OC1A Period: 0,13107 s Width: 65,793 ms
	
	TCCR1A=(1<<COM1A1) | (0<<COM1A0) | (0<<COM1B1) | (0<<COM1B0) | (0<<WGM11) | (1<<WGM10);
	TCCR1B=(0<<ICNC1) | (0<<ICES1) | (0<<WGM13) | (1<<WGM12) | (1<<CS12) | (0<<CS11) | (1<<CS10);
	TCNT1=0;
	
	initLCD();
	OWI_Init(BUS);
}
/***********************Прерывания********************************/

ISR(TIMER0_COMP_vect ){
	TCNT0=0;
	//cursorxy(0,5);
	//printf("item %d fl %d",selectedItem,flag.ts.ButtonIsPressedInISR);
	statePort=PIN_ENCODER&((1<<PIN_A_ENCODER)|(1<<PIN_B_ENCODER));
	if(statePort==0){
		regAB=128;//Как только А==В==0
	}
	else if(regAB!=0){
		switch(statePort){
			case AB_HIGH:
			if(regAB!=128){
				(*ptrInISR)-=(regAB<128)? -1:1;
				regAB=0;
			}
			break;
			
			case B_HIGH:
			regAB--;
			if(regAB<5) {(*ptrInISR)--;regAB=0;}
			break;
			
			case A_HIGH:
			regAB++;
			if(regAB>250) 	{(*ptrInISR)++;regAB=0;}
			break;
		}
	}
	
	
	
	
	if(BitIsSet(PIN_ENCODER,PIN_BIT_ENCODER_BUTTON)) flag.ts.ButtonIsPressed=0;
	
	if((BitIsClear(PIN_ENCODER,PIN_BIT_ENCODER_BUTTON))&&(flag.ts.ButtonIsPressed==0)){
		flag.ts.ButtonIsPressed=1;
		flag.ts.ButtonIsPressedInISR=1;
	}
	
	
}

/*
Настравиваемые параметры:
1.Верхний предел температуры //Предел t
2.Мощность
3.Выбег t
4.Ручное или автоматическое изменение мощности Ручн/Автомат
*/