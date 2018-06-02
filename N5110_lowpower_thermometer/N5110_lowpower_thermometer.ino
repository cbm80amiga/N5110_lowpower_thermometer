/*
 * (c)2017-18 Pawel A. Hernik
 * code for https://youtu.be/C0AbhEarWSc
 */
 
// *** CONNECTIONS ***
// N5110 LCD from left:
// #1 RST      - Pin 9
// #2 CS/CE    - Pin 10
// #3 DC       - Pin 8
// #4 MOSI/DIN - Pin 11
// #5 SCK/CLK  - Pin 13
// #6 VCC 3.3V
// #7 LIGHT (not used here)
// #8 GND
// thermistor from A0 to VCC or to any digital pin set to HIGH (to save power)
// balance resistor 10k from A0 to GND

// CURRENT MEASUREMENTS @ 16MHz:
//         5V         3V3     3XAAA  4xAAA   18650/100k  3xAAA
// V       4.8-4.49V  3.17V   3.53V  5.2     3.72V       3.81V
// WAKE    14.1mA     7.3mA   8.4mA  15.7mA  8.0mA       8.8mA
// SLEEP   175uA      595uA!  146uA  290uA   130uA       133uA
// power consumption - 11ms @ 7mA + 10*8s sleep @ 130uA
// http://www.of-things.de/battery-life-calculator.php
// for 1900mAh and 20% SDR it should work 500 days!

#define THERMISTOR_PIN A0
#define THERMISTOR_VCC 7
#define BALANCE_R 9980  // measure your 10kohm resistor

// uncomment to write min/max to EEEPROM
//#define USE_EEPROM

#include <avr/sleep.h>
#include <avr/wdt.h>
#include <EEPROM.h>

// define USESPI in LCD driver header for HW SPI version
#include "N5110_SPI.h"
#if USESPI==1
#include <SPI.h>
#endif
N5110_SPI lcd(9,10,8); // RST,CS,DC

#include "times_dig_16x24_font.h"
#include "c64enh_font.h"
#include "term9x14_font.h"

long readVcc() 
{
  long result;
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = 1125300L / result; // Back-calculate AVcc in mV
  return result;
}

float readTherm() 
{
  float pad = BALANCE_R;     // balance/pad resistor value = 10k
  //float thermR = 10000;    // thermistor nominal resistance = 10k
  long resist = pad*((1024.0 / analogRead(THERMISTOR_PIN)) - 1);
  float temp = log(resist);
  temp = 1 / (0.001129148 + (0.000234125 * temp) + (0.0000000876741 * temp * temp * temp));
  temp -= 273.15;
  return temp;
}

void wrFloat(float f, int addr)
{
#ifdef USE_EEPROM
  unsigned int ui = (f+100)*10;
  EEPROM.write(addr+0, ui&0xff);
  EEPROM.write(addr+1, (ui>>8)&0xff);
#endif
}

float rdFloat(int addr)
{
  unsigned int ui = EEPROM.read(addr) + (EEPROM.read(addr+1)<<8);
  return (ui/10.0)-100.0;
}

enum wdt_time {
	SLEEP_15MS,
	SLEEP_30MS,	
	SLEEP_60MS,
	SLEEP_120MS,
	SLEEP_250MS,
	SLEEP_500MS,
	SLEEP_1S,
	SLEEP_2S,
	SLEEP_4S,
	SLEEP_8S,
	SLEEP_FOREVER
};

ISR(WDT_vect) { wdt_disable(); }

void powerDown(uint8_t time)
{
  ADCSRA &= ~(1 << ADEN);  // turn off ADC
  if(time != SLEEP_FOREVER) { // use watchdog timer
    wdt_enable(time);
    WDTCSR |= (1 << WDIE);	
  }
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);  // most power saving
  cli();
  sleep_enable();
  sleep_bod_disable();
  sei();
  sleep_cpu();
  // ... sleeping here
  sleep_disable();
  ADCSRA |= (1 << ADEN); // turn on ADC
}

char buf[15],buf2[15];
float temp,mint=1000,maxt=-1000;

void setup() 
{
  //Serial.begin(9600);
  lcd.init();
  lcd.clrScr();
  for(int i=0;i<14;i++) pinMode(i, OUTPUT); 
#ifdef USE_EEPROM
  mint=rdFloat(0);
  maxt=rdFloat(2);
#endif
  //Serial.println(mint);
  //Serial.println(maxt);
  if(mint<-40 || mint>100) mint=1000;
  if(maxt<-40 || maxt>100) maxt=-1000;
  CLKPR = 0x80; // lower internal clock frequency to save power
  CLKPR = 0x02; // 0-16MHz, 1-8MHz, 2-4MHz, 3-2MHz, ..
}

void drawBatt(int x, int y, int wd, int perc)
{
  int w = wd*perc/100;
  lcd.fillWin(x,y,1+w,1,B01111111);
  x+=w+1;
  w=wd-w;
  if(w>0) {
    lcd.fillWin(x,y,w,1,B01000001);
    x+=w;
  }
  lcd.fillWin(x++,y,1,1,B01111111);
  lcd.fillWin(x++,y,1,1,B00011100);
  lcd.fillWin(x++,y,1,1,B00011100);
}

void loop() 
{
  digitalWrite(THERMISTOR_VCC, HIGH);  // enable thermistor
  temp = fabsf(readTherm());   // only positive values - room temperatures
  digitalWrite(THERMISTOR_VCC, LOW);  // disable thermistor to save power
  if(temp<mint) { mint=temp; wrFloat(mint,0); }
  if(temp>maxt) { maxt=temp; wrFloat(maxt,2); }
  dtostrf(temp,1,1,buf);
  char *c = strstr(buf,".");
  if(c) *c=':';
  lcd.setFont(times_dig_16x24);
  lcd.setDigitMinWd(17);
  lcd.printStr(8, 0, buf);
  lcd.setFont(Term9x14);
  lcd.printStr(68, 0, "`C");

  long v=readVcc();
  if(v<2900) {
    lcd.clrScr();
    lcd.printStr(28, 1, "Low");
    lcd.printStr(10, 3, "Battery");
    powerDown(SLEEP_8S);
    powerDown(SLEEP_8S);
    // disable LCD controller and power down forever to save battery
    lcd.sleep(true);
    powerDown(SLEEP_FOREVER);
  }
  lcd.setFont(c64enh);
  lcd.setDigitMinWd(6);
  dtostrf(v/1000.0,1,3,buf);
  drawBatt(8,5,20,constrain(map(v,2900,4200,0,100),0,100));
  lcd.printStr(36, 5, buf);
  lcd.printStr(70, 5, "V");

  buf[0]=0;
  strcat(buf,"<"); dtostrf(mint,1,1,buf2); strcat(buf,buf2);
  strcat(buf,"' >"); dtostrf(maxt,1,1,buf2); strcat(buf,buf2);
  strcat(buf,"'");
  lcd.printStr(4, 4, buf);
 
  // sleep for 10*8 seconds
  for(int i=0;i<10;i++)
    powerDown(SLEEP_8S);
}
