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

uint8_t  DS18B20_WriteScratchpad(uint8_t , uint8_t * , uint8_t , uint8_t ,uint8_t );
uint8_t DS18B20_ReadTemperature(uint8_t , uint8_t *, uint16_t* );
void send_SoundTextMessage( uint8_t );
void convertData(uint16_t *);
uint8_t controlPower(uint16_t,uint16_t);
uint8_t checkChange(uint8_t);
void settingPreferences(uint8_t);

OWI_device allDevices[MAX_DEVICES];
uint8_t rom[8];
uint8_t config_ds18b20[2]={0,0};
uint16_t e_tempHigh EEMEM = T_HIGH_START;



uint16_t selectedItem=1;
uint8_t itemMenu=0;
uint16_t  *ptrInISR;

uint16_t counter=0;
uint8_t regAB=0;

uint8_t statePort=0;
uint16_t tempHigh;//������� ������
uint16_t delta_runOutTemp=8;
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
	}ts; //ts ����������� this-����
	uint8_t byte;
}flag;



int main(void)
{
	//���������� printf ��� ������
	static FILE mystdout = FDEV_SETUP_STREAM(lcd_putchar, NULL,
	_FDEV_SETUP_WRITE);
	stdout=&mystdout;
	
	#ifdef _DEBUG
	DDRB=1<<DDB1 ;//| 1<<DDB3;
	#endif
	//TCCR0=(1<<WGM00) | (1<<COM01) | (0<<COM00) | (1<<WGM01) | (1<<CS02) | (0<<CS01) | (1<<CS00);
	//OCR0=OCR0_START;
	//
	//
	//TCCR1B=4;
	//TCNT1=0;
	//TIMSK=1<<OCIE1A;
	//OCR1A=8;
	
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
	OCR1A=OCR1A_START;
	
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
	
	/*���� ������ �����*/
	
	
	
	
	
	
	
	
	
	
	
	
	/*�������� ���������  */
	
	tempHigh=eeprom_read_word(&e_tempHigh);
	
	if(tempHigh==0xFFFF){
		eeprom_write_word(&e_tempHigh,T_HIGH_START);
		
		tempHigh=T_HIGH_START;
	}
	
	initLCD();
	
	OWI_Init(BUS);
	crcFlag = OWI_SearchDevices(allDevices, MAX_DEVICES, BUS, &iDevices);
	//��������� ���-�� �������� � CRC ��� ��
	if ((iDevices == MAX_DEVICES)&&(crcFlag != SEARCH_CRC_ERROR)){
		
		clearram();
		printf("��� ��");
		
		} else{
		clearram();
		printf("�-�� �������� %d",iDevices);
		send_SoundTextMessage(crcFlag);
		
	}
	for(uint8_t i=0;i<2;i++){
		while(DS18B20_WriteScratchpad(BUS, allDevices[i].id, 0, 0, DS18B20_12BIT_RES)!=WRITE_SUCCESSFUL);
	}
	
	
	
	while (1){
		//cursorxy(0,2);
		//printf("byte %d",flag.byte);
		
		if (flag.ts.Settings==0)
		{
			
			for(uint8_t i=0;i<2;){
				crcFlag = DS18B20_ReadTemperature(BUS, allDevices[i].id, &temperature[i]);
				if (crcFlag != READ_SUCCESSFUL){
					clearram();
					printf("������ %d",i+1);
					cursorxy(0,1);
					send_SoundTextMessage(crcFlag);
				}
				else{
					i++;
				}
			}
			
			averageTemperature=(temperature[0]+temperature[1])/2;
			
			
			//flag.ts.ValueHighIsChange ���� 0- valueHigh �� ����������, 1- ���������� �����
			//������ �� 1 ������ ��� ������ tempHigh
			flag.ts.Power=controlPower(averageTemperature,valueHigh);
			/*���������� ��������� ��������������� ������ */
			{
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
					
					if(count==0){
						lastValue=averageTemperature;
					}
					count++;
					if(count==10){
						count=0;
						if((lastValue==averageTemperature)&&(valueHigh!=tempHigh)){
							//����������� �������� ���� ��� ����-�� = �������
							//� ���� �� �������� �������� �������
							OCR1A+=((OCR1A<256)&&(manualOrAuto&1==1))? 1:0;
						}
					}
					
					//���� ����������� ������ ������ �� ����������� ��������
					if(averageTemperature < valueHigh-2*SHORT_LIMIT_TEMP){
						OCR1A+=((OCR1A<256-DELTA_PWM)&&(manualOrAuto&1==1))? DELTA_PWM:0;
						flag.ts.ValueHighIsChange=0;
						send_SoundTextMessage(TEMP_IS_DOWN);
					}
				}
				else{
					
					//��� ����������� �������� ����� ����������� run_out_temperature
					if(averageTemperature>=run_out_temperature){
						run_out_temperature=averageTemperature;
					}
					else if(flag.ts.RunOutIsCalculate==0){
						flag.ts.RunOutIsCalculate=1;
						flag.ts.ValueHighIsChange=0;
						if(run_out_temperature>valueHigh+delta_runOutTemp){
							OCR1A-=((OCR1A>delta)&&(manualOrAuto&1==1))? delta:0;//delta ��� ���������� ��������� ���
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
			printf("���� %d.%d",INTEGER(tempHigh),FRACTION(tempHigh));
			cursorxy(0,3);
			printf("Rt %d dT %d",run_out_temperature,run_out_temperature-valueHigh );
			cursorxy(0,4);
			printf("PWM %d Fl %d",OCR1A,flag.ts.ValueHighIsChange);
			cursorxy(0,5);
			printf("������� %d.%d",INTEGER(averageTemperature),FRACTION(averageTemperature));
			
			if(BitIsSet(PIN_ENCODER,PIN_BIT_ENCODER_BUTTON)) flag.ts.ButtonIsPressed=0;
			
			
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
			
			
			
			
			
			
			
			//����� �������� � ������
			while(1){
				if((checkChange(*ptrInISR)!=0)||(flag.ts.ButtonIsPressedInISR!=0)){
					//���� ��������� ���������
					//if((selectedItem>=5)&&(selectedItem<10)) selectedItem=0;
					//if(selectedItem>10)	selectedItem=4;
					
					if((*ptrInISR&0x8000)==0x8000 ){
						
						*ptrInISR=limitHigh;
					}
					if(*ptrInISR>limitHigh) *ptrInISR=0;
					
					settingPreferences(selectedItem);
					
					cursorxy(0,5);
					printf("item %x fl %d",*ptrInISR,flag.ts.Settings );
					break;
				}
			}
		}
	}
}
/*****************************************************************************/
uint8_t  DS18B20_WriteScratchpad(uint8_t bus, uint8_t * id, uint8_t th, uint8_t tl,uint8_t resolution){
	
	uint8_t scratchpad[9];
	uint8_t i;
	uint8_t flag=0;
	
	OWI_DetectPresence(bus);
	OWI_MatchRom(id, bus);
	OWI_SendByte(DS18B20_WRITE_SCRATCHPAD ,bus);
	OWI_SendByte(th ,bus);
	OWI_SendByte(tl ,bus);
	OWI_SendByte(resolution ,bus);
	//�������� ���������� �� ������������ ?
	OWI_DetectPresence(bus);
	OWI_MatchRom(id, bus);
	OWI_SendByte(DS18B20_READ_SCRATCHPAD, bus);
	for (i = 0; i<=8; i++){
		scratchpad[i] = OWI_ReceiveByte(bus);
	}
	flag=OWI_CheckScratchPadCRC(scratchpad);
	if(flag!= OWI_CRC_OK){
		return READ_CRC_ERROR;
	}
	
	if((scratchpad[2]!=th)||(scratchpad[3]!=tl)||((scratchpad[4]&0x60)!=resolution)){
		return WRITE_ERROR;
	}
	return WRITE_SUCCESSFUL;
	
}
/*****************************************************************************/
uint8_t DS18B20_ReadTemperature(uint8_t bus, uint8_t * id, uint16_t* temperature)
{
	#ifdef _DEBUG
	static uint8_t index=0;
	#endif
	uint8_t scratchpad[9];
	uint8_t i;
	uint8_t flag=0;
	
	/*������ ������ ������
	������� ��� ��������� ���������� �� ����
	������ ������� - ����� �������������� */
	OWI_DetectPresence(bus);
	OWI_MatchRom(id, bus);
	OWI_SendByte(DS18B20_CONVERT_T ,bus);

	/*����, ����� ������ �������� ��������������*/
	while (!OWI_ReadBit(bus));

	/*������ ������ ������
	������� ��� ��������� ���������� �� ����
	������� - ������ ���������� ������
	����� ��������� ���������� ������ ������� � ������
	*/
	OWI_DetectPresence(bus);
	OWI_MatchRom(id, bus);
	OWI_SendByte(DS18B20_READ_SCRATCHPAD, bus);
	for (i = 0; i<=8; i++){
		scratchpad[i] = OWI_ReceiveByte(bus);
	}
	flag=OWI_CheckScratchPadCRC(scratchpad);
	//printf("flag %d\n",flag);
	if(flag!= OWI_CRC_OK){
		return READ_CRC_ERROR;
	}
	
	*temperature = (uint16_t)scratchpad[0];
	*temperature |= ((uint16_t)scratchpad[1] << 8);
	if ((*temperature & 0x8000) == 1){
		return READ_NEGATIVE;
	}
	
	
	convertData(temperature);
	#ifdef _DEBUG
	config_ds18b20[index]=scratchpad[4];
	if(index!=1) index++;
	else index=0;
	#endif
	
	return READ_SUCCESSFUL;
}
/*****************************************************************************/
void send_SoundTextMessage( uint8_t code_error){
	
	clearram();
	switch(code_error){
		case READ_CRC_ERROR:
		cursorxy(0,1);
		printf("������ ������ ");
		break;
		case READ_NEGATIVE:
		printf("����� ����-��");
		break;
		
		case SEARCH_CRC_ERROR:
		printf("�� ������ CRC");
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
void convertData(uint16_t *_temperature){
	uint8_t fraction=0;
	fraction = (uint8_t)((*_temperature)&15);
	fraction=(uint8_t)(((uint64_t)fraction*625)/1000);
	*_temperature>>=4;
	*_temperature=(*_temperature)*10+fraction;
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
	uint8_t *string[]={" ������ t "," �������� "," ����� t "," ���/���� "," ����� "};
	
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
			}else{
			cli();
			flag.ts.Settings=0;
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
				case 3: ((manualOrAuto&1)==0)? printf("����"):printf("����");break;
			}
		}
		break;
		case 1:	printBigNumber(30,2,tempHigh,POINT);break;
		case 2:	printBigNumber(30,2,PERCENT_PWM(OCR1A),NO_POINT);break;
		case 3:	printBigNumber(30,2,delta_runOutTemp,POINT);break;
		case 4:
		
		cursorxy(30,2);
		((manualOrAuto&1)==0)? printf(" ������"):printf("�������");
		break;
		
	}
}
/***********************����������********************************/

ISR(TIMER0_COMP_vect ){
	TCNT0=0;
	//cursorxy(0,5);
	//printf("item %d fl %d",selectedItem,flag.ts.ButtonIsPressedInISR);
	statePort=PIN_ENCODER&((1<<PIN_A_ENCODER)|(1<<PIN_B_ENCODER));
	if(statePort==0){
		regAB=128;//��� ������ �==�==0
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
�������������� ���������:
1.������� ������ ����������� //������ t
2.��������
3.����� t
4.������ ��� �������������� ��������� �������� ����/�������
*/