/*
 * GccApplication1.c
 *
 * Created: 13.06.2020 18:42:29
 * Author : mitya
 */ 
#define F_CPU 1200000UL

#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <stdint.h>

volatile uint8_t send_counter = 0;
volatile uint32_t tick_counter = 0;
volatile uint8_t bit_to_send = 0;
volatile uint32_t next_send_tick = 1000;
volatile uint32_t next_led_tick = 5000;
volatile uint8_t button = 0; // 0-none, 1-left, 2-center, 3-right
volatile uint8_t rnd = 45;

const uint32_t sendaddrs[] = {3820897, 3820898, 3820900};

static inline void send_bit(void)
{
	if (bit_to_send) {
		if (PORTB & (1<<PORTB3)) {
			PORTB &= ~(1<<PORTB3);
			next_send_tick += (((sendaddrs[button-1] << (bit_to_send-1)) & 0x800000) > 0)?1:3;
			if (++bit_to_send > 24)
			{
				bit_to_send = 0;
				send_counter--;
			}
		} else {
			PORTB |= (1<<PORTB3);
			next_send_tick += (((sendaddrs[button-1] << (bit_to_send-1)) & 0x800000) > 0)?3:1;
		}
	} else { // sync bit
		if (PORTB & (1<<PORTB3)) {
			PORTB &= ~(1<<PORTB3);
			next_send_tick += 31;
			++bit_to_send;
		} else {
			PORTB |= (1<<PORTB3);
			next_send_tick += 1;
		}
	}
}


ISR(PCINT0_vect)
{
	// check what pin changed and do things
	if        (!(PINB & _BV(PINB0))) {
		button = 1;
	} else if (!(PINB & _BV(PINB1))) {
		button = 2;
	} else if (!(PINB & _BV(PINB2))) {
		button = 3;
	} else {
		button = 0;
	}
	if (button) {
		bit_to_send = 0;
		rnd = (0xa3 * rnd) % 254 + 1;
		next_send_tick = tick_counter + (rnd << 5); //wait upto 3 seconds before start sending (0-255 times 32-tick periods)
		send_counter = 22; //22 sends are 1 second
		set_sleep_mode(SLEEP_MODE_IDLE); // to allow timer run
		// Disable this interrupt
		GIMSK &= ~_BV(PCIE);
		// powerup radio
		PORTB |= _BV(PORTB4);
	}

}

// In CTC mode once when TIMSK0 == OCR0A
ISR(TIM0_COMPA_vect)
{
	cli();
	++tick_counter;
	if (send_counter && tick_counter >= next_send_tick) 
		send_bit();

	if (!send_counter) {
		// we sent all, disable transmitter, set heaviest sleep mode and enable PCINT till next button press
		PORTB &= ~(_BV(PORTB4));
		set_sleep_mode(SLEEP_MODE_PWR_DOWN);
		GIMSK |= _BV(PCIE);
	}
	sei();
}


static inline void setup(void)
{
	cli();
	OSCCAL = 60;

	DDRB = (1<<DDB3)|(1<<DDB4); // pin 2 and 3 out
	// pin 3 is Vcc for transmitter
	//PORTB = _BV(PORTB4);
	// Pullups on button pins
	PORTB = (1<<PORTB0)|(1<<PORTB1)|(1<<PORTB2);
	
	//set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	set_sleep_mode(SLEEP_MODE_IDLE);
	// Set timer to CTC mode
	TCCR0A = _BV(WGM01);
	// Set prescaler to 8
	TCCR0B = _BV(CS01);
	// interrupt every 353us at 1.2 MHz internal clock
	OCR0A = 53;
	// Overflow interrupt enable 
	TIMSK0 = _BV(OCIE0A);
	
	// Enable interrupt on pins 5, 6, 7 change
	PCMSK = _BV(PCINT0)|_BV(PCINT1)|_BV(PCINT2);
	// Enable PC Interrupt
	GIMSK = _BV(PCIE);

	sei();
}

int main(void)
{
	setup();
    while (1) { sleep_mode(); }
}
