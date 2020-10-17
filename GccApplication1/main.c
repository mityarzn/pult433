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
volatile uint16_t tick_counter = 0;
volatile uint8_t bit_to_send = 0;
volatile uint16_t next_send_tick = 1000;
volatile uint8_t button = 0; // 0-none, 1-left, 2-center, 3-right

const uint32_t sendaddrs[] = {3820897, 3820898, 3820900};

static inline uint8_t selectaddr(void)
{
	uint8_t idx;
	if (button == 2) { // Special behavior for middle button: 5 sends right, 12 both, 5 left
		if (send_counter >= 18) {
			idx = 3;
		} else if (send_counter >= 6)	{
			idx = 2;
		} else {
			idx = 1;
		}
	} else if (button == 1) {
		idx = 1;
	} else {
		idx = 3;
	}
	return idx;
}

static inline void send_bit(void)
{
	if (bit_to_send) {
		uint8_t idx = selectaddr();
		if (PORTB & _BV(PORTB3)) {
			PORTB &= ~_BV(PORTB3);
			next_send_tick += (((sendaddrs[idx] << (bit_to_send-1)) & 0x800000) > 0)?1:3;
			if (++bit_to_send > 24)
			{
				bit_to_send = 0;
				send_counter--;
				if (!send_counter) {
					// We had sent all. Disable transmitter, reset button number
					button = 0;
					PORTB |= _BV(PORTB4);
				}

			}
		} else {
			PORTB |= _BV(PORTB3);
			next_send_tick += (((sendaddrs[idx] << (bit_to_send-1)) & 0x800000) > 0)?3:1;
		}
	} else { // sync bit
		if (PORTB & _BV(PORTB3)) {
			PORTB &= ~_BV(PORTB3);
			next_send_tick += 31;
			++bit_to_send;
		} else {
			PORTB |= _BV(PORTB3);
			next_send_tick += 1;
		}
	}
}

/*
ISR(PCINT0_vect)
{

}
*/

// In CTC mode once when TIMSK0 == OCR0A
ISR(TIM0_COMPA_vect)
{
	++tick_counter;
	if (button && send_counter && tick_counter >= next_send_tick) 
		send_bit();
}


static inline void setup(void)
{
	cli();
	OSCCAL = 60;

	DDRB = _BV(DDB3)|_BV(DDB4); // pin 2 and 3 out
	// Pullups on button pins
	//PORTB = _BV(PORTB0)|_BV(PORTB1)|_BV(PORTB2);
	PORTB |= _BV(PORTB4); // power key is inverted. Disable by default.
	
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
	//PCMSK = _BV(PCINT0)|_BV(PCINT1)|_BV(PCINT2);
	// Enable PC Interrupt
	//GIMSK = _BV(PCIE);

	sei();
}

int main(void)
{
	uint8_t rnd = 45;
	setup();
	while (1) {
		cli();
		if (!button) {
			// check what pin changed and do things
			if        ((PINB & _BV(PINB0))) {
				button = 1;
			} else if ((PINB & _BV(PINB1))) {
				button = 2;
			} else if ((PINB & _BV(PINB2))) {
				button = 3;
			} else {
				button = 0;
			}
		}
		if (button && (!send_counter)) {
			bit_to_send = 0;
			rnd = (0xa3 * rnd * (uint8_t) tick_counter) + 1;
			// prevent overflow on wait for send
			if (tick_counter > 0xff00)
			{
				tick_counter = 0;
			}
			next_send_tick = tick_counter + ((uint16_t) rnd << 5); //wait up to 3 seconds before start sending (0-255 times 32-tick periods)
			send_counter = 22; //22 sends are 1 second
			set_sleep_mode(SLEEP_MODE_IDLE); // to allow timer run
			PORTB &= ~_BV(PORTB4); // powerup radio
		}
		sei();
		sleep_mode();
	}
}
