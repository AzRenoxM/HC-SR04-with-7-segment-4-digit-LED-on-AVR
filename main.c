#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define F_CPU	1000000UL

static volatile uint16_t	pulse		= 0;
static volatile uint8_t		edge_state	= 0;

const uint8_t SEGMENT_MAP_L[] = {
	0b11111000, // 0	next A in PB0: 0b1
	0b11000000, // 1	next A in PB0: 0b0
	0b10110010, // 2	next A in PB0: 0b1
	0b11100010, // 3	next A in PB0: 0b1
	0b11001010, // 4	next A in PB0: 0b0
	0b01101010, // 5	next A in PB0: 0b1
	0b01111010, // 6	next A in PB0: 0b1
	0b11000000, // 7	next A in PB0: 0b1
	0b11111010, // 8	next A in PB0: 0b1
	0b11101010, // 9	next A in PB0: 0b1
};
const uint8_t SEGMENT_MAP_H[] = {
	0b00000001, // 0 	PB0
	0b00000000, // 1 	PB0
	0b00000001, // 2 	PB0
	0b00000001, // 3 	PB0
	0b00000000, // 4 	PB0
	0b00000001, // 5 	PB0
	0b00000001, // 6 	PB0
	0b00000001, // 7 	PB0
	0b00000001, // 8 	PB0
	0b00000001	// 9 	PB0
};

void const_init(void){
	DDRC	&= 0xE8; // 0b11101000		// NL NL NL Trig NL S3 S2 S1 
	DDRC	|= 0x10; // 0b00010000
	PORTC	&= 0xF8; // 0b11111000		// pull up 
	PORTC	|= 0x07; // 0b00000111

	DDRD	= 0xFB;	 // 0b11111011;		// B C D E F INT0 G DP
	PORTD	= 0xFB;	 // 0b11111011;		// pull up, since common Anode
	
	DDRB	&= 0xE0; // 0b11100000;		// 4 digit display numbers position + A
	DDRB	|= 0x1F; // 0b00011111;		// 4 digit display numbers position + A
	
	PORTB	&= 0xE0; // 0b00011111;		// pull up, since common Anode
	PORTB	|= 0x1F; // 0b00011111;		// pull up, since common Anode
	
	EICRA	|= (1 << ISC00);			// setting interrupt triggering ANY logic change
	EIMSK	|= (1 << INT0);				// enabling INT0
	sei();								// enable INT globally
	
	TCCR1A = 0;				// timer config
	TCCR1B = 0;				// timer config
}

void number_display(int number){
	if (number > 9999) number = 9999;
	int place = 1;		// digit pins starts from PINB1 not 0
	if (number == 0) {
		display_digit(0, place);
		_delay_ms(15);
		return;
	}
	while(number > 0 && place <= 4){
		int unit = number % 10;
		display_digit(unit, place);
		number = number / 10;
		place++;
	}
}

void display_digit(uint8_t number, int place){
	PORTB |= ((1 << PINB4) | (1 << PINB3) | (1 << PINB2) | (1 << PINB1));	// clear
	
	// reset the LED screen, here PIND2 is EXTINT
	PORTD |= 0xFB;			// 0b11111011
	PORTB |= (1 << PINB0);	// here also reset of A
	
	PORTD &= ~SEGMENT_MAP_L[number];
	PORTB &= ~SEGMENT_MAP_H[number];
	PORTB &= ~(1 << place);
	_delay_ms(4);
}

int Trigger_sensor(){
	PORTC |= (1 << PORTC4);
	_delay_us(10);
	PORTC &= ~(1 << PORTC4);
}

void check_speed(uint16_t *speed_loop){
	if(!(PINC & (1 << PINC0))){
		*speed_loop = 10;
	} else if (!(PINC & (1 << PINC1))){
		*speed_loop = 1000;
	} else if (!(PINC & (1 << PINC2))){
		*speed_loop = 65535;
	}
}

int main(void){
	const_init();
	uint16_t loop_counter = 0;
	int distance_cm = 0;
	uint16_t speed_loop = 1000;
	while(1){
		number_display(distance_cm);
		loop_counter++;
		check_speed(&speed_loop);
		
		if (loop_counter >= speed_loop && speed_loop != 65535) {
			cli();
            uint16_t current_pulse = pulse;
            sei();
			
			distance_cm = current_pulse / 58;
            Trigger_sensor();
            loop_counter = 0;
		}
	}
}

ISR(INT0_vect){
	if (edge_state == 0){				// HIGH to LOW 
		if (PIND & (1 << PIND2)) {
			TCNT1 = 0;					// resetting the memory of the counter
			TCCR1B |= (1 << CS10);		// Start timer, no prescaler in this case
			edge_state = 1;				
		}
	} else if (edge_state == 1){		// LOW to HIGH
		if (!(PIND & (1 << PIND2))) {	// Verify pin is actually Low
			TCCR1B = 0;					// Stop Timer
			pulse = TCNT1;				// Save time value
			edge_state = 0;				// Reset state
		}
	}
}

