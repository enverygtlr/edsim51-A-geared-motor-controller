#include <reg51.h>
#include <inttypes.h>
#include <string.h>

#include <stdlib.h>

#include <lcd.h>
#include <stdio.h>

#define BUFFER_SIZE 10
#define MAX_SPEED 100


void print_constant( uint8_t* str);

void Ext_int_Init()
{
	EA = 1; /* Enable global interrupt */
    EX0 = 1;
    IT0 = 1;
    ET0 = 1;
	ES = 1; /* Enable Ext. interrupt0 */
}

void disable()
{
	EA = 0;
    EX0 = 0;
    ET0 = 0;
	ES = 0;
}

void enable()
{
	EA = 1;
    EX0 = 1;
    ET0 = 1;
	ES = 1;
}

void initTimers()
{
	TMOD = 0x21; /* Timer 1, 8-bit auto reload mode  Timer 0 mode 2 reloadf*/
	TH1 = 0xF4; /* Load value for 2400 baud rate */
    TH0 = 0xFF;
	TL0 = 0xF0;			 // 10 us period
	SCON = 0x50; /* Mode 1, reception enable */
	TR1 = 1; /* Start timer 1 */
    TR0 = 1;
}


volatile uint8_t is_input_full = 0;
volatile uint8_t  input_buffer[BUFFER_SIZE];
uint8_t input_buffer_index = 0;


uint8_t newline_entered = 0;
uint8_t send = 1;
uint8_t received = 0;
void Serial_ISR() interrupt 4
{
	if(TI==1){
		send = 1;
		TI = 0; /* Clear TI flag */
	}
	if(RI==1){
		uint8_t in = SBUF;				
		if(input_buffer_index < BUFFER_SIZE)
		{
			input_buffer[input_buffer_index] = in;
			input_buffer_index++;
				
			if(in == '\n')
			{
				if(input_buffer_index < BUFFER_SIZE)
					input_buffer[input_buffer_index] = '\0';
				else
					input_buffer[input_buffer_index - 1] = '\0';
				
				newline_entered = 1;
				input_buffer_index = 0;
			}
		}
		else{
			is_input_full  = 1;
			input_buffer_index = 0;	
		}
		received = 1;
		RI = 0; /* Clear RI flag */
	}
}

int desiredSpeed = 0;
int keypadValue = 1;
unsigned int measure;
int iterationCount = 0;

int revolution = 0;
sbit LED = P1^0;
sbit LED2 = P1^1;
int  printFlag = 0;
int  curSpeed = 0;
int  prevSpeed = 0;
int  curAcceleration = 0;


sbit Abit = P3^3;
sbit Bbit = P3^4;


void External0_ISR() interrupt 0
{
    revolution++;
} 

int calcSpeedFlag = 0;
int calcSpeedCounter = 0;
void Timer0_ISR() interrupt 1
{
	LED = ~LED;	/* Toggle pin on falling edge on INT0 pin */

	if(iterationCount < desiredSpeed)
	{
		Abit = 0;
		Bbit = 1;
	}
	else if(iterationCount >= desiredSpeed)
	{
		Abit = 0;
		Bbit = 0;
	}

	if(iterationCount >= MAX_SPEED)
	{
		iterationCount = 0;
	}

	iterationCount++;

	calcSpeedCounter++;
	if(calcSpeedCounter >= 100)
	{
		calcSpeedFlag = 1;
		calcSpeedCounter = 0;
	}
    TH0 = 0xFF;
	TL0 = 0xF0;	
}


void print_constant( uint8_t* str)
{
	uint8_t ch;
	uint8_t i;
	i = 0;
	ch = str[0];
	while(ch != '\0')
	{
		disable();
		send = 0;
		enable();

		SBUF = ch;
		while(!send);
		i++;
		ch = str[i];
	}
}


sbit INTR = P3^5;
uint8_t  read_adc()
{
	uint8_t measure;
	P2 = 0xFF;
	RD = 0; 			//rd line low
	WR = 0;
	WR = 1; 				//initiate adc transmission
	while(INTR != 0);	//wait transmission to finish
	measure = P2;
	return measure;
}


sbit row0 = P0^0;
sbit row1 = P0^1;
sbit row2 = P0^2;
sbit row3 = P0^3;
sbit col0 = P0^4;
sbit col1 = P0^5;
sbit col2 = P0^6;

sbit switch0 = P0^7;

int colScan(int* keyValue)
{
	if(col0 == 0)
	{
		return 1;
	}
	(*keyValue)++;

	if (col1 == 0)
	{
		return 1;
	}
	(*keyValue)++;
	
	if (col2 == 0)
	{
		return 1;
	}
	(*keyValue)++;

	return 0;
}

int searchKey()
{
	int keyValue;

	keyValue = 0;

	row3 = 1;
	row0 = 0;
	if(colScan(&keyValue))
	{
		row0 = 1;
		return keyValue;
	}
	row0 = 1;
	row1 = 0;
	if(colScan(&keyValue))
	{
		row1 = 1;
		return keyValue;
	}
	row1 = 1;
	row2 = 0;
	if(colScan(&keyValue))
	{
		row2 = 1;
		return keyValue;
	}
	row2 = 1;
	row3 = 0;
	if(colScan(&keyValue))
	{
		row3 = 1;
		return keyValue;
	}
	return -1;	
}

int idata overrideGear = 0;
unsigned int idata overrideSpeed = 0;
int idata overrideFlag = 0;
int idata acceptMessage = 0;

int keypadVal = -1;
int  gearVal = 0;
unsigned int speedVal = 0;

char buffer[10];
char idata messageBuffer[10];


int checkParity(char* message)
{
	char parityStr[4];
	int parityVal;
	int sumOfChars = 0;
	int i;
	for (i = 0; i < 5; i++)
	{
		sumOfChars+= (int) message[i];
	}
	memcpy(parityStr, &message[5], 3);
	parityStr[3] = '\0';
	parityVal = atoi(parityStr);
	
	if(parityVal == (sumOfChars % 255))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}


void main()
{

	Ext_int_Init(); 
	initTimers();


	functionSet();
	entryModeSet(1, 0);
	displayOnOffControl(1, 1, 1); 

  
	while(1)
    {
		if(switch0 == 0)
		{
			acceptMessage = 1;
		}
		else
		{
			overrideFlag = 0;
			acceptMessage = 0;
		}
		

		//keypad task
		keypadVal = searchKey();
		if(keypadVal != -1)
		{
			if(keypadVal == 1)
				keypadVal = 0;
			else
			{
					keypadVal = 12 - keypadVal;
			}
			gearVal = keypadVal;
			// sprintf(buffer, "key: %d\n", keypadVal);
			// print_constant(buffer);
		}

		//serial task
		if(newline_entered)
		{
			char op;
			newline_entered = 0;
			disable();
			memcpy(messageBuffer, input_buffer, BUFFER_SIZE);
			enable();
			
			if(acceptMessage)
			{
				op = messageBuffer[0];

				if(checkParity(messageBuffer))
				{
					if(op == 'O')
					{
						char idata speedChar[4];
						char idata gearChar;
						memcpy(speedChar, &messageBuffer[1], 3);
						speedChar[3] = '\0';
						gearChar = messageBuffer[4];

						overrideSpeed = atoi(speedChar);
						overrideGear = (int) gearChar - 48;

						// sprintf(buffer, "%d\n", overrideSpeed);
						// print_constant(buffer);

						// sprintf(buffer, "%d\n", overrideGear); 
						// print_constant(buffer);

						overrideFlag = 1;
						print_constant("ACK\n");
					}
					if(op == 'S')
					{
						overrideFlag = 0;
						print_constant("ACK\n");
					}
				}
				else
				{
					print_constant("NACK\n");
				}

			}
			else
			{
				print_constant("NACK\n");
			}		

		}

		//motor task
		if(calcSpeedFlag)
		{
			long int idata tmp;
		    int locDesiredSpeed;
			calcSpeedFlag = 0;
			speedVal = (unsigned int) read_adc();

			// sprintf(buffer, "g:%d\n", gearVal);
			// print_constant(buffer);

			// sprintf(buffer, "s:%u\n", speedVal);
			// print_constant(buffer);
			if(overrideFlag)
			{
				tmp =  (long int) (overrideSpeed * overrideGear)*MAX_SPEED; 
			}
			else
			{
				tmp = (long int) (speedVal*gearVal)*MAX_SPEED;
			}

			disable();
			desiredSpeed = tmp/2295;
			locDesiredSpeed = desiredSpeed;
			enable();
 			// sprintf(buffer, "d:%u\n", locDesiredSpeed);
			// print_constant(buffer);

			disable();
			curSpeed = revolution;
			revolution = 0;
			enable();

			curAcceleration = curSpeed - prevSpeed;
			prevSpeed = curSpeed;
			 
            // sprintf(buffer, "%d 1/ms\n", curSpeed);
            // print_constant(buffer);

			//LCD task
			sprintf(buffer, "%d   ", curSpeed);
			returnHome();
			sendString(buffer);
			setDdRamAddress(0x40); 
			sprintf(buffer, "%d   ", curAcceleration);
			sendString(buffer);

	

		}

		

    }
}