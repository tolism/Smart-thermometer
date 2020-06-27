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
volatile int tempRead=0;                       //Interrupt -> Read Temperature every 5 seconds
volatile int newSecond=0;                         //Interrupt -> Update the LCD Screen every second
volatile int readDistance=0;                          //Interrupt -> Calculate the distance every 200ms
volatile int secondsCounter=0;                      //Interrupt -> Auxilary variable for calculating the above




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

	leds_init();
	lcd_init();
	lcd_clear();
	timer_init(200000);
	timer_set_callback(timer_isr);
	timer_enable();
	gpio_set_mode(TRIGGER_PIN, Output);
	gpio_set_mode(ECHO_PIN, Input);
	temperature = get_temperature();

	while(1){

		if(newSecond==1){
		newSecond = 0;
	updateLCD(message);

	}


	//Get distance value every 200ms
	if(readDistance ==1){
		readDistance = 0;
		distance_time = distance_read();
		distance = distance_time * .034/2;
		printf(" distance = %d cm\n", distance);
	}

	//Get temperature value every 5s
	if(tempRead==1){
		temperature = get_temperature();
		printf(" The temperature is %f \n", temperature);

		temperaturesArray[temperaturesArrayIdx]= temperature;
		if(temperaturesArrayIdx == 23){
			avgTemperature =  0 ;
      //calculate the average temperature
			for(int i = 0 ; i < 24 ; i ++){
				avgTemperature += temperaturesArray[i];
			}
			avgTemperature = avgTemperature/24;
			temperaturesArrayIdx = 0;
		}
		else{
			temperaturesArrayIdx++;
		}

	}

  //Check for High/Low Temperatures
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
	if (distance <= dist && avgTemperature !=0) {                                        //If user close to device
		message = 1 ;
	}
	else if(distance > dist && temperaturesArrayIdx>2 && temperaturesArrayIdx<24){  //Print LEDs' state (main LCD message)
	  message = 4;
	}
	else if(distance > dist && temperaturesArrayIdx<=2 && avgTemperature == 0  ){   //Wait for the first mean temp to be calculated
   message = 4 ;
  }
	else if(distance < dist  && avgTemperature == 0  ){
    message = 3 ;
		temperatureState = 4;
	}
	else{                                                         //New mean temp is calculated, print for 10 secs
   message = 2;
	}
	}

}


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


void init_timer(){
	if (ARM_CM_DWT_CTRL != 0) {      // See if DWT is available
		ARM_CM_DEMCR      |= 1 << 24;  // Set bit 24

		ARM_CM_DWT_CYCCNT = 0;

		ARM_CM_DWT_CTRL   |= 1 << 0;   // Set bit 0
	}
}




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

//ena apo ta 2 den prepei na xreiazete
	start = ARM_CM_DWT_CYCCNT;

	while(gpio_get(ECHO_PIN));

	end = ARM_CM_DWT_CYCCNT;
	total_cycles = end - start;
	total_time = total_cycles / (SystemCoreClock * 1e-6);

	return 2*total_time;
}


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

