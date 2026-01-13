/*
 * main.c
 *
 * Created: 1/9/2026 1:37:51 PM
 *  Author: Justus
 */ 
#define F_CPU 10000000UL// 10 MHZ clock
#include <xc.h>
#include <util/delay.h>
#include <avr/interrupt.h>



#define Button1 PIN7_bm  // PORTA
#define Button2 PIN6_bm  // PORTA
#define LedPin PIN2_bm   // PORTA
#define PWM PIN3_bm		 // PORTB
#define Hall PIN4_bm	 // PORTA 

// Display

#define  CLK PIN0_bm // PORTB
#define DIO PIN1_bm // PORTB

volatile int8_t speedMode = 0; 
int8_t lastMode = 0;
uint8_t buttonflag = 0;
volatile uint16_t Pulses = 0;
uint16_t RPM = 0; 
uint8_t skiptime = 0; // Per 100ms

const uint8_t digitMap[10] = {
	0x3F, // 0
	0x06, // 1
	0x5B, // 2
	0x4F, // 3
	0x66, // 4
	0x6D, // 5
	0x7D, // 6
	0x07, // 7
	0x7F, // 8
	0x6F  // 9
};

void TM_sendByte(uint8_t data){
	for(int i = 0; i<8; i++){
		
		PORTB.OUTCLR = CLK;
		
		if (data & (1<<i)) // if value at index (lsb first)
		{
			PORTB.OUTSET = DIO;
		}
		else {
			PORTB.OUTCLR = DIO;
		}
		
		PORTB.OUTSET = CLK;
	}
	
	// WAIT FOR ACK
	PORTB.OUTCLR = CLK;
	PORTB.DIRCLR = DIO; // To input
	PORTB.OUTSET = CLK;
	_delay_ms(1);
	PORTB.OUTCLR = CLK;
	PORTB.DIRSET = DIO; // Back to Output
}

void TM_start(void){
	PORTB.OUTSET = CLK | DIO;
	PORTB.OUTCLR = DIO;
	PORTB.OUTCLR = CLK;
}

void TM_stop(void){
	PORTB.OUTCLR = CLK;
	PORTB.OUTCLR = DIO;
	PORTB.OUTSET = CLK;
	PORTB.OUTSET = DIO;
}
void TM_displayInit(void){
	TM_start();
	TM_sendByte(0x40);  // auto increment
	TM_stop();

	TM_start();
	TM_sendByte(0x88 | 0x07); // display ON, max brightness
	TM_stop();
}

void TM_printNumber(uint16_t num)
{
	uint8_t digits[4];

	if (num > 9999) num = 9999;

	digits[0] = digitMap[(num / 1000) % 10];
	digits[1] = digitMap[(num / 100)  % 10];
	digits[2] = digitMap[(num / 10)   % 10];
	digits[3] = digitMap[num % 10];

	TM_start();
	TM_sendByte(0xC0);      // Start form 0
	for (uint8_t i = 0; i < 4; i++)
	TM_sendByte(digits[i]);
	TM_stop();
	
}

void setSpeed(uint16_t us){
	TCA0.SINGLE.CMP0 = (uint32_t)us * 625 / 1000;
}

void setup(void){
	// Setup cpu clockspeed
	CPU_CCP = CCP_IOREG_gc; // Write protection off
	CLKCTRL.MCLKCTRLB = CLKCTRL_PEN_bm | CLKCTRL_PDIV_2X_gc; // 20 MHz clk, 2x Prescale = 10 MHZ

	
	// Buttons to input
	PORTA.DIRCLR = Button1;
	PORTA.DIRCLR = Button2;
	// Button internal pull-ups
	PORTA.PIN7CTRL = PORT_PULLUPEN_bm;
	PORTA.PIN6CTRL = PORT_PULLUPEN_bm;
	// Button interrupts
	PORTA.PIN7CTRL |= PORT_ISC_FALLING_gc;
	PORTA.PIN6CTRL |= PORT_ISC_FALLING_gc;
	// On board led output
	PORTA.DIRSET = LedPin;
	// Hall input from motor sensor
	PORTA.DIRCLR = Hall; // Hall pin to output
	PORTA.PIN4CTRL = PORT_ISC_FALLING_gc;
	// TM 1637 display
	PORTB.DIRSET = CLK; // Both to output
	PORTB.DIRSET = DIO;
	PORTB.OUTSET = CLK; // Both high
	PORTB.OUTSET = DIO;
	// Configure PWM output for ESC
	PORTB.DIRSET = PWM; // PWM pin to output
	PORTMUX_CTRLC = PORTMUX_TCA00_ALTERNATE_gc; // WO0 to alternate pin (PB3)

	TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_SINGLESLOPE_gc | TCA_SINGLE_CMP0EN_bm;
	TCA0.SINGLE.PER = 12500;
	TCA0.SINGLE.CMP0 = 625;
	TCA0.SINGLE.CTRLA = TCA_SINGLE_ENABLE_bm | TCA_SINGLE_CLKSEL_DIV16_gc;
	
	TM_displayInit();
	sei();
	PORTA.OUTSET = LedPin; // Led on
}

ISR(PORTA_PORT_vect){
	if (PORTA.INTFLAGS & Hall)
	{
		Pulses++;
		PORTA.OUTTGL = LedPin;
	}
	else
	{
		buttonflag = 1;
	}
	PORTA.INTFLAGS = 0xff;
}

int main(void)
{
	setup();
	TM_printNumber(0000);
	_delay_ms(1000);
    while(1)
    {
		if(buttonflag){
			buttonflag = 0;
			uint8_t buttons = PORTA.IN;
			if(!(buttons & Button1)){
				speedMode++;
			}
			if(!(buttons & Button2)){
				speedMode--;
			}
			if (speedMode < 0){		 // Prevent mode from going negative
				speedMode = 0;
			}
		}
		
		if(lastMode != speedMode){			// Display mode for one second when changed
			lastMode = speedMode;
			skiptime = 0;
		}
		
		if (skiptime < 10)
		{
			TM_printNumber(speedMode);
			skiptime++;
		}
		else
		{
			TM_printNumber(RPM);
		}
		
		setSpeed(1000+speedMode*100);
		
		RPM = (uint16_t) Pulses / 0.1 * 60;
		Pulses = 0;
		
		_delay_ms(100);
		
    }
}