#ifndef MODULE_DS18B20
#define MODULE_DS18B20

#include "ds18b20/OWIPolled.h"
#include "ds18b20/OWIHighLevelFunctions.h"
#include "ds18b20/OWIBitFunctions.h"
#include "ds18b20/OWIcrc.h"

//код семейства и коды команд датчика DS18B20
#define DS18B20_FAMILY_ID                0x28
#define DS18B20_CONVERT_T                0x44
#define DS18B20_READ_SCRATCHPAD          0xbe
#define DS18B20_WRITE_SCRATCHPAD         0x4e
#define DS18B20_COPY_SCRATCHPAD          0x48
#define DS18B20_RECALL_E                 0xb8
#define DS18B20_READ_POWER_SUPPLY        0xb4

//вывод, к которому подключены 1Wire устройства
#define BUS   OWI_PIN_2

//количество устройств на шине 1Wire
#define MAX_DEVICES       2

//коды сообщений для функции чтения температуры
#define READ_SUCCESSFUL   0x00
#define READ_CRC_ERROR    0x01
#define READ_NEGATIVE     0x02
#define WRITE_ERROR       0x03
#define WRITE_SUCCESSFUL  0x04
#define TEMP_IS_DOWN	  0x06
#define TEMP_IS_STABLE	  0x07
#define FEWER_DEVICES	  0x08	

#define SEARCH_SENSORS 0x00
#define SENSORS_FOUND 0xff

#define DS18B20_9BIT_RES 0  // 9 bit thermometer resolution
#define DS18B20_10BIT_RES 32 // 10 bit thermometer resolution
#define DS18B20_11BIT_RES 64 // 11 bit thermometer resolution
#define DS18B20_12BIT_RES 96 // 12 bit thermometer resolution

#define  T_HIGH_START		370
//Перенести в отд начальные параметры 
#define  OCR1A_START		128
#define  DELTA_PWM			10
#define  SHORT_LIMIT_TEMP	10 //Предел температуры в градусах*10
#define  DELTA_RUN_OUT_TEMP_START 8 //Начальный выбег температуры
#define  CONTRAST_LCD		0x40
#endif