#include "ch32fun.h"
#include <stdio.h>

void loop() __attribute__((section(".srodata")));

int main()
{
	SystemInit();
  Delay_Ms(50);

	funGpioInitAll();
	funPinMode(PA1, FUN_OUTPUT);
  funDigitalWrite(PA1, FUN_HIGH);

  loop();
}

void loop() {
	while(1)
	{
		funDigitalWrite(PA1, FUN_HIGH);
		Delay_Ms(100);
		funDigitalWrite(PA1, FUN_LOW);
		Delay_Ms(100);
	}
}

