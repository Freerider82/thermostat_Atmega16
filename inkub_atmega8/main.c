/*
* inkub_atmega8.c
*
* Created: 06.04.2018 14:13:56
* Author : Oleg
*/

#include "project_inkub.h"
#include "module_ds18b20.h"
#include <avr/wdt.h>
//#include "myUart.h" //����� ���������� ��� �������� atmega328p



#include <avr/interrupt.h>

//#define  _DEBUG



void send_SoundTextMessage( uint8_t );
uint8_t controlPower(uint16_t,uint16_t);
uint8_t checkChange(uint8_t);
void settingPreferences(uint8_t);
void init_mc(void);

uint16_t e_tempHigh EEMEM = T_HIGH_START;
uint16_t  e_valuePWM EEMEM = OCR1B_START;
uint8_t  e_deltaRunOutTemp EEMEM = DELTA_RUN_OUT_TEMP_START;
uint8_t  e_manualOrAuto EEMEM = 0;
uint8_t  e_contrastLCD = CONTRAST_LCD;

uint16_t selectedItem=1;
uint8_t itemMenu=0;
uint16_t  *ptrInISR;

uint16_t counter=0;
uint8_t regAB=0;

uint8_t statePort=0;
uint16_t tempHigh;//������� ������
uint16_t delta_runOutTemp; //����� �����������
uint16_t limitHigh=0;
uint16_t manualOrAuto=0;
uint16_t contrastLcd=60;

union bitByte{
	struct {
		unsigned ValueHighIsChange:1;
		unsigned RunOutIsCalculate:1;
		unsigned Power:1;
		unsigned Settings:1;
		unsigned ButtonIsPressed:1;
		unsigned ButtonIsPressedInISR:1;
	}ts; //ts ����������� this-����
	uint8_t byte;
}flag;



int main(void)
{
	//���������� printf ��� ������
	static FILE mystdout = FDEV_SETUP_STREAM(lcd_putchar, NULL,
	_FDEV_SETUP_WRITE);
	stdout=&mystdout;
	
	
	
	OWI_device allDevices[MAX_DEVICES];
	
	uint16_t temperature[2] = {0,0};
	
	uint16_t averageTemperature;
	uint8_t iDevices = 0;
	uint8_t crcFlag = 0;
	
	uint16_t run_out_temperature=0;//����� ����������� ��-�� ���������� �����������
	
	uint16_t valueHigh=0;
	uint8_t count=0;
	uint16_t lastValue;
	uint8_t delta=0;
	
	flag.byte=0; //��� ����� � 0
	wdt_enable(WDTO_8S);
	init_mc();	
		
		
	
	
	{
		//������ �� ������ EEPROM ��������� ��� ������������� ��������
		tempHigh=eeprom_read_word(&e_tempHigh);
		
		if(tempHigh==0xFFFF){
			eeprom_write_word(&e_tempHigh,T_HIGH_START);
			eeprom_write_word(&e_valuePWM,OCR1B_START);
			eeprom_write_byte(&e_deltaRunOutTemp,DELTA_RUN_OUT_TEMP_START);
			eeprom_write_byte(&e_manualOrAuto,0);
			eeprom_write_byte(&e_contrastLCD,CONTRAST_LCD);
		}
		
		tempHigh=eeprom_read_word(&e_tempHigh);
		delta_runOutTemp=eeprom_read_byte(&e_deltaRunOutTemp);
		OCR1B=eeprom_read_word(&e_valuePWM);
		manualOrAuto=eeprom_read_byte(&e_manualOrAuto);
		contrastLcd=eeprom_read_byte(&e_contrastLCD);
		//TO DO ������� ������� ��������� ��������� � ���� ��� 
		contrastLCD(contrastLcd);
		
		//�����������  ���-�� ��������, ������ �� �������� � allDevices
		crcFlag = OWI_SearchDevices(allDevices, MAX_DEVICES, BUS, &iDevices);
		if(iDevices!=MAX_DEVICES) {
			crcFlag=FEWER_DEVICES;		
			send_SoundTextMessage(crcFlag);
			cursorxy(0,1);
			printf("�-�� �-��� %d",iDevices);	
			while(1){wdt_reset();}	//��������� � ���� ����� ���� ���-�� �������� <2
			
		}
		//��������� �������� �� ���.����������
		for(uint8_t i=0;i<MAX_DEVICES;i++){
			while(DS18B20_WriteScratchpad(BUS, allDevices[i].id, 0, 0, DS18B20_12BIT_RES)!=WRITE_SUCCESSFUL);
		}
		
		while (1){
			wdt_reset();
			if (flag.ts.Settings==0)
			{
				//������ ����������� 2 ��������
				for(uint8_t i=0;i<MAX_DEVICES;){
					crcFlag = DS18B20_ReadTemperature(BUS, allDevices[i].id, &temperature[i]);
					if (crcFlag != READ_SUCCESSFUL){
						send_SoundTextMessage(crcFlag);
						cursorxy(0,1);
						printf("�-��� %d",i+1);
						ClearBit(DDR_POWER,PIN_POWER);
						wdt_reset();
					}
					else{
						i++;
					}
				}
				
				averageTemperature=(temperature[0]+temperature[1])/2;
				
				
				//
				flag.ts.Power=controlPower(averageTemperature,valueHigh);
				/*���������� ��������� � �������������� ������ */
				{
					//flag.ts.ValueHighIsChange ���� 0- valueHigh �� ����������,
					// 1- ���������� ����� ������ �� 1 (SHORT_LIMIT_TEMP) ������ ��� ������ tempHigh
					// ����� ValueHighIsChange ��� ��������� ������ �����������
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
						//������� ��� �������� ������������ �����������
						if(count==0){
							lastValue=averageTemperature;
						}
						count++;
						if(count==10){
							count=0;
							//���� ����������� �� �������� � ������� ������ != ������������� � ���������� �������
							if((lastValue==averageTemperature)&&(valueHigh!=tempHigh)){
								//����������� �������� ���� ��� ����-�� = �������
								//� ���� �� �������� �������� �������
								OCR1B+=((OCR1B<1023)&&(manualOrAuto&1==1))? 1:0;
							}
						}
						
						//���� ����������� ������ ������ �� ����������� ��������
						if(averageTemperature < valueHigh-2*SHORT_LIMIT_TEMP){
							OCR1B+=((OCR1B<1024-DELTA_PWM)&&(manualOrAuto&1==1))? DELTA_PWM:0;
							flag.ts.ValueHighIsChange=0;
							send_SoundTextMessage(TEMP_IS_DOWN);
						}
					}
					else{
						//flag.ts.Power==POWER_OFF
						//��� ����������� �������� ����� ����������� run_out_temperature
						if(averageTemperature>=run_out_temperature){
							run_out_temperature=averageTemperature;
						}
						else if(flag.ts.RunOutIsCalculate==0){
							flag.ts.RunOutIsCalculate=1;
							flag.ts.ValueHighIsChange=0;
							if(run_out_temperature>valueHigh+delta_runOutTemp){
								OCR1B-=((OCR1B>delta)&&(manualOrAuto&1==1))? delta:0;//delta ��� ��������� ��������� ���
							}
						}
					}
				}
				
				clearram();
				cursorxy(0,0);
				printf("%d.%d 1�",INTEGER(temperature[0]),FRACTION(temperature[0]));
				cursorxy(0,1);
				printf("%d.%d 2�",INTEGER(temperature[1]),FRACTION(temperature[1]));
				cursorxy(0,2);
				printf("%d.%d ��",INTEGER(averageTemperature),FRACTION(averageTemperature));
				cursorxy(0,3);
				printf("%d.%d ��",INTEGER(tempHigh),FRACTION(tempHigh));
				cursorxy(0,4);
				printf("%d.%d %d.%d",INTEGER(run_out_temperature-valueHigh),FRACTION(run_out_temperature-valueHigh),INTEGER(delta_runOutTemp),FRACTION(delta_runOutTemp) );
				cursorxy(0,5);
				printf("�: %d",PERCENT_PWM(OCR1B));
				
				
				//���� ������ �������� �� ������
				if(BitIsSet(PIN_ENCODER,PIN_BIT_ENCODER_BUTTON)) flag.ts.ButtonIsPressed=0;
				
				//���� ������ �������� ������ ��������� � ����� ��������
				if((BitIsClear(PIN_ENCODER,PIN_BIT_ENCODER_BUTTON))&&(flag.ts.ButtonIsPressed==0)){
					ClearBit(DDR_POWER,PIN_POWER);
					itemMenu=0;
					flag.ts.Settings=1;
					flag.ts.ButtonIsPressedInISR=0;
					flag.ts.ButtonIsPressed=1;
					limitHigh=5;
					selectedItem=4;//� ���������� �������� �� ���� �������� (����� � ������ ���� ������ ������ �����)
					ptrInISR=&selectedItem;
					clearram();
					settingPreferences(selectedItem);
					sei();
				}
			}
			else{
				//flag.ts.Settings=1
				//����� �������� � ������
				while(1){
					//���� ���������� ������� ��� ������ �� ������
					if((checkChange(*ptrInISR)!=0)||(flag.ts.ButtonIsPressedInISR!=0)){
						//���� ptrInISR ���������� �� ������������� �����
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
	
	//_delay_ms(1000);
	////int8_t tempature = 0;
	//int8_t humidity = 0;
	//while(1) {
	//humidity = dht11_gethumidity();
	////tempature = dht11_gettemperature();
	//
	////cursorxy(0,0);
	////printf("temp %d",tempature);
	//cursorxy(0,1);
	//printf("���� %d",humidity);
	//SetBit(PORTB,1);
	//_delay_ms(500);
	//ClearBit(PORTB,1);
	////clearram();
	//}
}

/*****************************************************************************/
void send_SoundTextMessage( uint8_t code_Message){
	
	clearram();
	switch(code_Message){
		
		case SEARCH_SUCCESSFUL:
		printf("��� ��");
		break;
		
		case READ_CRC_ERROR:
		cursorxy(0,1);
		printf("������ ������ ");
		break;
		
		case FEWER_DEVICES:
		printf("�������� ����");
		break;
		
		case READ_NEGATIVE:
		printf("����� ����-��");
		break;
		
		case SEARCH_CRC_ERROR:
		printf("�� ������ CRC");
		break;
		
		case SEARCH_ERROR:
		printf("��� �������");
		break;
		
		case TEMP_IS_DOWN:
		printf("�����������   ������");
		break;
		
		case TEMP_IS_STABLE:
		printf("�����������   ����������");
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
	uint8_t *string1[]={"������ t","���-���","����� t","���/���","�������","�����"};
	uint8_t *string[]={"�t","����","� t","�/�","���","�����"};
	uint8_t flagPrint=NOT_INVERSION;
	if(flag.ts.ButtonIsPressedInISR==1)
	{//���� ������ ������ � ������ ���� , ������� �������� 
		
			clearram();
			cursorxy(0,0);
			printf(string1[item]);
			
			
			if(itemMenu==0)
			{
				itemMenu=item+1;
				 
				switch(item)
				{
					case 0: limitHigh=1270;ptrInISR=&tempHigh;break;
					case 1: limitHigh=1022;ptrInISR=&OCR1B;break;
					case 2: limitHigh=100;ptrInISR=&delta_runOutTemp;break;
					case 3: limitHigh=1;ptrInISR=&manualOrAuto;break;
					case 4: limitHigh=128;ptrInISR=&contrastLcd;break;
					case 5: 
					cli();
					flag.ts.Settings=0;
					eeprom_update_word(&e_tempHigh,tempHigh);
					eeprom_update_word(&e_valuePWM,OCR1B);
					eeprom_update_byte(&e_deltaRunOutTemp,delta_runOutTemp);
					eeprom_update_byte(&e_manualOrAuto,manualOrAuto);
					eeprom_update_byte(&e_contrastLCD,contrastLcd);
					break;
				}
			}else
			 {
				 //����� ������� �� ������ ������� �� ������� � �������� ����
				limitHigh=5;
				ptrInISR=&selectedItem;
				itemMenu=0;
			 }
		
		flag.ts.ButtonIsPressedInISR=0;
	}
	switch(itemMenu){
		//itemMenu=0 ��� ����� ����
		case 0:
		clearram();
		for (uint8_t y=0;y<6;y++){
			cursorxy(0,y);
			flagPrint=(item==y)? INVERSION:NOT_INVERSION;
			lcd_print(string[y],flagPrint);
			switch(y){
				case 0: printf("%d.%d",INTEGER(tempHigh),FRACTION(tempHigh));break;
				case 1: printf("%d",PERCENT_PWM(OCR1B));break;
				case 2: printf("%d.%d",INTEGER(delta_runOutTemp),FRACTION(delta_runOutTemp));break;
				case 3: ((manualOrAuto&1)==0)? printf("����"):printf("���");break;
				case 4: printf("%d",contrastLcd);break;
			}
		}
		break;
		// ��������� case ��� ������� �� ���� ��������� ���������, ����� �������� �������� �������
		case 1:	printBigNumber(30,2,tempHigh,POINT);break;
		case 2:	printBigNumber(30,2,PERCENT_PWM(OCR1B),NO_POINT);break;
		case 3:	printBigNumber(30,2,delta_runOutTemp,POINT);break;
		case 4: 		
		cursorxy(0,2);
		((manualOrAuto&1)==0)? printf("������ "):printf("�������");
		break;
		case 5:printBigNumber(30,2,contrastLcd,NO_POINT);contrastLCD(contrastLcd);break;
		
		
	}
}
/*****************************************************************************/
void init_mc(){
	
	#ifdef _DEBUG
	//DDRB=1<<DDB1 ;
	#endif
	
	// Timer/Counter 0 initialization
	// Clock source: System Clock
	// Clock value: 15,625 kHz
	// Mode: Normal top=0xFF
	// OC0A output: Disconnected
	// OC0B output: Disconnected
	// Timer Period: 16,384 ms
	
	TCCR0B=(0<<WGM02) | (1<<CS02) | (0<<CS01) | (1<<CS00);
	TCNT0=0x00;
	OCR0A=16;//���������� ����� 1 ��
	// Timer/Counter 0 Interrupt(s) initialization
	TIMSK0=(0<<OCIE0B) | (1<<OCIE0A) | (0<<TOIE0);
	
	// Timer/Counter 1 initialization
	// Clock source: System Clock
	// Clock value: 15,625 kHz
	// Mode: Fast PWM top=0x03FF
	// OC1A output: Disconnected
	// OC1B output: Non-Inverted PWM
	// Noise Canceler: Off
	// Input Capture on Falling Edge
	// Timer Period: 65,536 ms
	// Output Pulse(s):
	// OC1B Period: 65,536 ms Width: 0 us
	// Timer1 Overflow Interrupt: Off
	// Input Capture Interrupt: Off
	// Compare A Match Interrupt: Off
	// Compare B Match Interrupt: Off
	TCCR1A=(0<<COM1A1) | (0<<COM1A0) | (1<<COM1B1) | (0<<COM1B0) | (1<<WGM11) | (1<<WGM10);
	TCCR1B=(0<<ICNC1) | (0<<ICES1) | (0<<WGM13) | (1<<WGM12) | (1<<CS12) | (0<<CS11) | (1<<CS10);
	TCNT1H=0x00;
	TCNT1L=0x00;
	ICR1H=0x00;
	ICR1L=0x00;
	OCR1AH=0x00;
	OCR1AL=0x00;
	OCR1BH=0x00;
	OCR1BL=0x00;
	TCNT1=0;	
	
	
	initLCD();	
	
	OWI_Init(BUS);
}
/***********************����������********************************/

ISR(TIMER0_COMPA_vect ){
	TCNT0=0;
	//cursorxy(0,5);
	//printf("item %d fl %d",selectedItem,flag.ts.ButtonIsPressedInISR);
	statePort=PIN_ENCODER&((1<<PIN_A_ENCODER)|(1<<PIN_B_ENCODER));
	if(statePort==0){
		regAB=128;//��� ������ �==�==0
	}
	else if(regAB!=0){
		wdt_reset();
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
�������������� ���������:
1.������� ������ ����������� //������ t
2.��������
3.����� t
4.������ ��� �������������� ��������� �������� ����/�������
*/