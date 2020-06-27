/*
* FILE: main.c
* THMMY, 8th semester, Microprocessors & Peripherals
*  
* Authors:
*   Moustaklis Apostolos, 9127, amoustakl@ece.auth.gr
*   Papadakis Charis , 9128, papadakic@ece.auth.gr
*
* A smart thermometer with user interaction 
* 
*/

#include <platform.h>
#include <delay.h>
#include <stdio.h>
#include <gpio.h>
#include <leds.h>
#include <lcd.h>
#include <i2c.h>
#include <timer.h>


//Pins define
#define TRIGGER_PIN PA_0
#define ECHO_PIN PA_1
#define TEMP_PIN PA_4

//Critical temperature and distance
#define TEMP_LOW  20;
#define TEMP_HIGH  30;
#define DIST  40;

// ARM_CM  to calculate number of clock cycles
#define  ARM_CM_DEMCR      (*(uint32_t *)0xE000EDFC)
#define  ARM_CM_DWT_CTRL   (*(uint32_t *)0xE0001000)
#define  ARM_CM_DWT_CYCCNT (*(uint32_t *)0xE0001004)


//Global variables 
uint16_t distance;
uint32_t distance_time;

int temp_low = TEMP_LOW;
int temp_high = TEMP_HIGH;
float dist = DIST;

uint8_t presence;
uint8_t temp_byte1;
uint8_t temp_byte2;
float temp;
float temperature;
int message ;

float temperaturesArray[24];
int  temperaturesArrayIdx  = 0;
float avgTemperature ;
int temperatureState ;
int switchButton ;


char * overHeat = "H"; // High Temperature
char * underHeat = "L"; //Low Temperature
char * normalHeat = "N"; //Normal Temperature
char * startMessage = "Please wait"; //Wait for the first measurement


//Volatile variables changed at the execution time
volatile int tempRead=0;           //Temperature read every 5 seconds
volatile int newSecond=0;          //LCD Screen update every 1 second
volatile int readDistance=0;       //Distance calculation every 200ms
volatile int secondsCounter=0;     //Variable that calculates seconds  




//Function decleration
void init_timer(void);
uint32_t distance_read(void);
float tempReaderature(void);
uint8_t temperature_start(void);
void temperature_write(uint8_t data);
uint8_t temperature_read(void);
void timer_isr(void);
void updateLCD(int message);
void heat_state_print(int message);
float get_temperature(void);


int main(void){
//Initialization 
	leds_init();
	lcd_init();
	lcd_clear();
	timer_init(200000);
	timer_set_callback(timer_isr);
	timer_enable();
	gpio_set_mode(TRIGGER_PIN, Output);
	gpio_set_mode(ECHO_PIN, Input);
//Get the temperature for the first LCD update 
	temperature = get_temperature();

	while(1){

//Update the LCD screen every second 		
		if(newSecond==1){
		newSecond = 0;
	updateLCD(message);

	}


	//Get the distance value every 200ms
	if(readDistance ==1){
		readDistance = 0;
		distance_time = distance_read();
		distance = distance_time * .034/2;
		printf(" distance = %d cm\n", distance);
	}

	//Get the temperature value every 5s
	if(tempRead==1){
		temperature = get_temperature();
		temperaturesArray[temperaturesArrayIdx]= temperature;
		//If we get the 24 measurements 
		if(temperaturesArrayIdx == 23){
			avgTemperature =  0 ;
     //Calculating the average temperature
			for(int i = 0 ; i < 24 ; i ++){
				avgTemperature += temperaturesArray[i];
			}
			avgTemperature = avgTemperature/24;
			temperaturesArrayIdx = 0;
		}
		//Increase the counter to the next index 
		else{
			temperaturesArrayIdx++;
		}

	}

  //Check for High/Normal/ Low Temperatures
if(temperature > temp_high){
	leds_set(1,0,0);
	temperatureState = 1;
	switchButton = 1 ;

}
else if (temperature < temp_low){
	leds_set(0,0,1);
	temperatureState = 2 ;
	switchButton  = 0 ;

}
else{
	leds_set(0,1,0);
	temperatureState = 3 ;
  switchButton = 0 ;
}

	//Find the message to be printed
	if (distance <= dist && avgTemperature !=0) {            //If the user is close to the device
		message = 1 ;
	}
	else if(distance > dist && temperaturesArrayIdx>2 && temperaturesArrayIdx<24){  //Print LEDs state (
	  message = 4;
	}
	else if(distance > dist && temperaturesArrayIdx<=2 && avgTemperature == 0  ){   //Wait for the first mean temp to be calculated
   message = 4 ;
  }
	else if(distance < dist  && avgTemperature == 0  ){    //Please wait for the first average temperature measurement 
    message = 3 ;
		temperatureState = 4;
	}
	else{                                                         //Print the average temperature for 10seconds
   message = 2;
	}
	}

}


//Helper functions Implementation

//Function to get the temperature
float get_temperature(void){
			tempRead = 0 ;
		presence = temperature_start();
		delay_ms(1);
		temperature_write(0xCC);
		temperature_write(0x44);
		delay_ms(1);

		presence = temperature_start();
		delay_ms(1);
		temperature_write(0xCC);
		temperature_write(0xBE);

		temp_byte1 = temperature_read();
		temp_byte2 = temperature_read();


		temp = (temp_byte2<<8) | temp_byte1;
	return 	temperature = (float)temp / 16;
}

//Function to update the LCD panel
void updateLCD(int message){

		char str[20] = {0};
		char str2[20] = {0};
switch (message){
	case 1 :

		lcd_set_cursor(0,1);
		sprintf(str, "Temp: %.2f ",temperature);
		lcd_print(str);
		lcd_print("C");
		lcd_set_cursor(0,0);
		sprintf(str2, "Average :%.2f ",avgTemperature);
		lcd_print(str2);
		lcd_print("C");
		lcd_set_cursor(15,1);
		heat_state_print(temperatureState);
			 break;

	case 2 :
			lcd_clear();
			lcd_set_cursor(0,0);
			sprintf(str,"Average :%.2f ",avgTemperature);
			lcd_print(str);
			lcd_print("C");
			lcd_set_cursor(0,1);
			heat_state_print(temperatureState);
			break;

		case 3:
		lcd_clear();
		lcd_set_cursor(0,0);
		heat_state_print(temperatureState);

		 break;

		default:
		lcd_clear();
		lcd_set_cursor(0,0);
		heat_state_print(temperatureState);

	}

}

//Function to print the state ( H/N/L ) at the LCD panel
void heat_state_print(int message){
	switch(message){
		case 1:
			lcd_print(overHeat);
		 break;

		case 2 :
			lcd_print(underHeat);
		break;

		case 3 :
			lcd_print(normalHeat);
			break;

		case 4 :
			lcd_print(startMessage);
			break;



	}
}


//Timer isr function
void timer_isr(void)
{
	secondsCounter = secondsCounter+1;
	if (secondsCounter == 25){
		tempRead=1;
		secondsCounter = 0;
	}
	if (secondsCounter%5==0){
		newSecond=1;
	}
	readDistance = 1;
}


//Initialization of the timer used to get the distance from the sensor 
void init_timer(){
	if (ARM_CM_DWT_CTRL != 0) {      // See if DWT is available
		ARM_CM_DEMCR      |= 1 << 24;  // Set bit 24

		ARM_CM_DWT_CYCCNT = 0;

		ARM_CM_DWT_CTRL   |= 1 << 0;   // Set bit 0
	}
}

//Function to read the distance from the sensor
uint32_t distance_read() {
	init_timer();
	uint32_t start;
	uint32_t end;
	uint32_t total_cycles;
	uint32_t total_time;



	gpio_set(TRIGGER_PIN, 1);
	delay_us(10);
	gpio_set(TRIGGER_PIN, 0);

	while(!(gpio_get(ECHO_PIN)));

	start = ARM_CM_DWT_CYCCNT;

	while(gpio_get(ECHO_PIN));

	end = ARM_CM_DWT_CYCCNT;
	total_cycles = end - start;
	total_time = total_cycles / (SystemCoreClock * 1e-6);

	return 2*total_time;
}

//Function to start communicating with the temperature sensor
uint8_t temperature_start(void) {
	int response = 0;
	gpio_set_mode(TEMP_PIN, Output);
	gpio_set(TEMP_PIN, 0);
	delay_us(480);

	gpio_set_mode(TEMP_PIN, Input);
	delay_us(80);

	if(!(gpio_get(TEMP_PIN))){
		response = 1;
	}
	else{
		response = -1;
	}
	delay_us(400);

	return response;
}

//Function to write to the temperature sensor the command to execute
void temperature_write(uint8_t data) {

	gpio_set_mode(TEMP_PIN, Output);

	for(int i=0; i<8; i++) {
		if((data & (1<<i)) !=0 ){
			gpio_set_mode(TEMP_PIN, Output);
			gpio_set(TEMP_PIN, 0);
			delay_us(1);

			gpio_set_mode(TEMP_PIN, Input);
			delay_us(40);
		}
		else {
			gpio_set_mode(TEMP_PIN, Output);
			gpio_set(TEMP_PIN, 0);
			delay_us(40);

			gpio_set_mode(TEMP_PIN, Input);

		}
	}
}

//Function to read from the temperature sensor
uint8_t temperature_read(void) {

	uint8_t value = 0;
	gpio_set_mode(TEMP_PIN, Input);

	for(int i=0; i<8; i++) {
		gpio_set_mode(TEMP_PIN, Output);
		gpio_set(TEMP_PIN, 0);
		delay_us(2);

		gpio_set_mode(TEMP_PIN, Input);
		if(gpio_get(TEMP_PIN)) {
			value |= 1<<i;
		}

		delay_us(60);
	}

	return value;
}

