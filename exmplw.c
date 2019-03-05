/*
**	usb_sprite.c
**
**	Control a Sprite on Teensy via joystick or USB serial.
**
**	Lawrence Buckingham, QUT, October 2017.
**	(C) Queensland University of Technology.
*/
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <avr/io.h> 
#include <avr/interrupt.h>
#include <util/delay.h>
#include <cpu_speed.h>
#include <macros.h>
#include <graphics.h>
#include <sprite.h>

#include "usb_serial.h"

typedef enum { false, true } bool;

void setup(void);
void setup_hero(void);
void setup_joystick(void);
void setup_usb_serial(void);
void usb_serial_send(char * message);
void usb_serial_send_int(int value);
void process(void);

//-------------------------------------------------------------------

int main(void) {
	setup();

	for ( ;; ) {
		process();
	}
}
//-------------------------------------------------------------------

Sprite hero;

void process(void) {
	int dx = 0, dy = 0;

	if ( BIT_IS_SET(PIND, 1) ) dy = -1;
	if ( BIT_IS_SET(PINB, 7) ) dy = 1;
	if ( BIT_IS_SET(PINB, 1) ) dx = -1;
	if ( BIT_IS_SET(PIND, 0) ) dx = 1;

	if ( usb_serial_available() ) {
		int c = usb_serial_getchar();
		if ( c == 'a' ) dx = -1;
		if ( c == 'd' ) dx = +1;
		if ( c == 's' ) dy = +1;
		if ( c == 'w' ) dy = -1;
	}

	if ( dx || dy ) {
		hero.x += dx;
		hero.y += dy;
		clear_screen();
		sprite_draw(&hero);
		show_screen();
		usb_serial_send("Moved hero to (");
		usb_serial_send_int((int)hero.x);
		usb_serial_putchar(',');
		usb_serial_send_int((int)hero.y);
		usb_serial_send(")\r\n");
	}
}

//-------------------------------------------------------------------

void setup(void) {
	set_clock_speed(CPU_8MHz);
	lcd_init(LCD_DEFAULT_CONTRAST);
	setup_usb_serial();
	setup_hero();
	setup_joystick();
}

//-------------------------------------------------------------------

void setup_hero(void) {
	static uint8_t bitmap[] = {
		0b00111100,
		0b01000010,
		0b10100101,
		0b10000001,
		0b10100101,
		0b10011001,
		0b01000010,
		0b00111100,
	};
	int W = 8, H = 8;
	sprite_init(&hero, (LCD_X - W) / 2, (LCD_Y - H) / 2, W, H, bitmap);
	clear_screen();
	sprite_draw(&hero);
	show_screen();
}

//-------------------------------------------------------------------

void setup_joystick(void) {
	CLEAR_BIT(DDRD, 1);
	CLEAR_BIT(DDRB, 7);
	CLEAR_BIT(DDRB, 1);
	CLEAR_BIT(DDRD, 0);
}

// ---------------------------------------------------------
//	USB serial business.
// ---------------------------------------------------------

void setup_usb_serial(void) {
	// Set up LCD and display message
	lcd_init(LCD_DEFAULT_CONTRAST);
	draw_string(10, 10, "Connect USB...", FG_COLOUR);
	show_screen();

	usb_init();

	while ( !usb_configured() ) {
		// Block until USB is ready.
	}

	clear_screen();
	draw_string(10, 10, "USB connected", FG_COLOUR);
	show_screen();

	usb_serial_send("Please press W A S D to control the sprite!\r\n");
}

/*
**	Transmits a string via usb_serial.
*/
void usb_serial_send(char * message) {
	// Cast to avoid "error: pointer targets in passing argument 1 
	//	of 'usb_serial_write' differ in signedness"
	usb_serial_write((uint8_t *) message, strlen(message));
}

/*
**	Transmits an integer value via usb_serial.
*/

void usb_serial_send_int(int value) {
	static char buffer[8];
	snprintf(buffer, sizeof(buffer), "%d", value);
	usb_serial_send( buffer );
}

//-------------------------------------------------------------------
