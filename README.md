# N5110_lowpower_thermometer
Low power Arduino thermometer

This project can work for 500 days using 3xAA/AAA or any lithium cell

Features:
- extremely low power with modded Pro Mini
- ATMEGA clock speed reduced to 4MHz
- display is refreshed every 80 seconds
- most optimal power option: any lithium cell or 3xNiMH AA/AAA, at battery capacity 2000mAh thermometer should work for about 500 days
- there is no lithium protection in hardware, but current cell voltage is displayed and in case of less than 3V warning message is shown and everything enters deep sleep mode to avoid further discharging
- thermistor isn't connected directly between ADC and VCC to avoid constant power consumption
- temperature min/max and battery level are displayed

Use N5110_SPI lib from my github
