#ifndef PROJECT_INKUB
#define PROJECT_INKUB
//Частота 
#define F_CPU 2000000UL //1MHz

#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <avr/eeprom.h>

#include "bits_macros.h"
#include "driver_nokia5110.h"
#include "dht11.h"

// Мощность упр ШИМ поэтому вкл/откл DDR выход/вход
#define DDR_POWER	DDRD
#define PIN_POWER	DDD5
#define POWER_ON	1
#define POWER_OFF	0

#define PIN_ENCODER		PIND
#define PIN_BIT_ENCODER_BUTTON PD2	
#define PIN_A_ENCODER	PD3
#define PIN_B_ENCODER	PD4
#define AB_HIGH			(1<<PIN_A_ENCODER)|(1<<PIN_B_ENCODER)
#define A_HIGH			1<<PIN_A_ENCODER
#define B_HIGH			1<<PIN_B_ENCODER

#define PERCENT_PWM(x)  ((x)*100)/256
#endif