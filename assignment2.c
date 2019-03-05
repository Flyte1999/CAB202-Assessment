#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <avr/io.h> 
#include <avr/interrupt.h>
#include <util/delay.h>
#include <cpu_speed.h>
#include <graphics.h>
#include <stdbool.h>
#include <macros.h>
#include <lcd_model.h>
#include <lcd.c>
#include <ascii_font.h>
#include <sprite.h>
#include "usb_serial.h"

char buffer[20];

static const unsigned char blocks_safe[] PROGMEM = {
        0b11111111, 0b11000000,
        0b11111111, 0b11000000
};

static const unsigned char blocks_forbid[] PROGMEM = {
        0b10101010, 0b10101010,
        0b01010101, 0b01010101
};

static const unsigned char treasure[] PROGMEM = {
    0b11100000, 
    0b10100000,
    0b11100000
};

static const unsigned char player[] PROGMEM = {
        0b01000000,
        0b11100000,
        0b01000000,
        0b10100000
};

Sprite allBlocksSprite[21];
Sprite player_sprite;
Sprite treasure_sprite;

double timeOfPress = 0.0;
double timeOfPress_up = 0.0;
double cur_time = 0.0;
double difference = 0.0;
double speed = 0.0;
double flash_time = 0.0;


int block_y = 10;
int block_x = 10;
int total_forbid = 0;
int total_safe = 0;
int lives = 10;
int score = 0;
int spawn_x = 13;
int spawn_y = 5;
int mins = 0.0;
int secs = 0.0;
int player_direction = 1;
int block_direction[21];
int c;


bool paused = false;
bool paused_pressed = false;
bool toggle = true;
bool treasure_dir = true;
bool introCheck = false;
bool allBlocks[21];
bool end = false;
bool back = true;
bool scored = true;
bool fast_fall = true;
bool spawn = false;
bool treasure_message = false;
bool game_over = false;


//create volatile variable
volatile uint32_t overflow_count = 0;
//

//interrupt service routine to process timer overflow
ISR(TIMER1_OVF_vect){
    overflow_count++;
}

void usb_check(){
    if ( usb_serial_available() ) {
		c = usb_serial_getchar();
    }
    else {
        c = 0;
    }
}

void usb_serial_send(char * message) {
	usb_serial_write((uint8_t *) message, strlen(message));
}

void usb_serial_send_int(int value) {
	static char buffer[8];
	snprintf(buffer, sizeof(buffer), "%d", value);
	usb_serial_send( buffer );
}

void draw_double(uint8_t x, uint8_t y, double value, colour_t colour) {
	snprintf(buffer, sizeof(buffer), "%2.f", value);
	draw_string(x, y, buffer, colour);
}

double current_time(){
    double time;
    time = (overflow_count * 65536.0 + TCNT1) * (1/8000000.0);
    return time;
}

bool sprite_step( sprite_id sprite ) {
    int x0 = round( sprite->x );
    int y0 = round( sprite->y );
    sprite->x += sprite->dx;
    sprite->y += sprite->dy;
    int x1 = round( sprite->x );
    int y1 = round( sprite->y );
    return ( x1 != x0 ) || ( y1 != y0 );
}

void enable_inputs(void)
{
    // Joystick
    CLEAR_BIT(DDRB, 1); // Left
    CLEAR_BIT(DDRD, 0); // Right
    CLEAR_BIT(DDRD, 1); // Up
    CLEAR_BIT(DDRB, 7); // Down
    CLEAR_BIT(DDRB, 0); // Centre

    //Switches
    CLEAR_BIT(DDRF, 6); // SW2
    CLEAR_BIT(DDRF, 5); // SW3

    //LEDS
    CLEAR_BIT(DDRB, 2); // LED 0
    CLEAR_BIT(DDRB, 3); // LED 1
}

void setup_pot(){
    ADMUX |= 1 << REFS0;
    ADCSRA |= 1 << ADEN | 1 << ADPS2 | 1 << ADPS1 | 1 << ADPS0;
}

int16_t read_pot(){
    ADMUX &= 0b11111000;
    ADMUX |= (1 & ~(0xF8));
    ADCSRA |= 1 << ADSC;
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

void block_speed(){
    double adc = read_pot();
    speed = (adc/6.82666667)/100;
}

void introScreen(){
    sprite_init(&player_sprite, spawn_x, spawn_y, 3, 4, load_rom_bitmap(player, 4));
    free(load_rom_bitmap(player, 4));
    sprite_init(&treasure_sprite, 0, 45, 3, 3, load_rom_bitmap(treasure, 3));
    free(load_rom_bitmap(treasure, 3));
    while(introCheck == false){
    draw_string(0, 0, "Mason Trout", FG_COLOUR);
    draw_string(0, 10, "N10170791", FG_COLOUR);
    draw_string(0, 20, "Press SW2 to", FG_COLOUR);
    draw_string(0, 30, "Begin!", FG_COLOUR);
        if ((c == 's') || (BIT_VALUE (PINF, 6))) {
        introCheck = true;
        }
        show_screen();
    }
}

void block_check(){
    for (int i = 0; i < 21; i++){
        if(allBlocksSprite[i].x < 51 && allBlocksSprite[i].x > 9 && allBlocksSprite[i].y == 10 && allBlocks[i] == true){
            spawn_x = allBlocksSprite[i].x;
            spawn_y = allBlocksSprite[i].y - player_sprite.height;
        } else if (allBlocksSprite[i].x < 51 && allBlocksSprite[i].x > 9 && allBlocksSprite[i].y == 10 && allBlocks[i] == false){
            spawn_x = allBlocksSprite[(i-1)].x;
            spawn_y = allBlocksSprite[i].y - player_sprite.height;
        }
    }
} 

void pauseMenu(){
    if ((( c == 'p') || (BIT_VALUE ( PINB , 0 ))) && paused == false  && (current_time() > (timeOfPress + 1) )) {
        paused = true;  
        paused_pressed = true;    
        timeOfPress = cur_time;
        clear_screen();
        while ( (( c == 'p') || (BIT_VALUE ( PINB , 0 ))) && paused == true ) {
            usb_check();
            draw_string(0, 0, "Lives: ", FG_COLOUR);
            draw_double(28, 0, lives, FG_COLOUR);
            draw_string(0, 10, "Time: ", FG_COLOUR);
            draw_double(24, 10, mins, FG_COLOUR);
            draw_string(36, 10, ":", FG_COLOUR);
            draw_double(40, 10, secs, FG_COLOUR);
            draw_string(0, 20, "Score: ", FG_COLOUR);
            draw_double(30, 20, score, FG_COLOUR);
            show_screen();
        } 

        while ( paused == true ) {
            usb_check();
            draw_string(0, 0, "Lives: ", FG_COLOUR);
            draw_double(28, 0, lives, FG_COLOUR);
            draw_string(0, 10, "Time: ", FG_COLOUR);
            draw_double(24, 10, mins, FG_COLOUR);
            draw_string(36, 10, ":", FG_COLOUR);
            draw_double(40, 10, secs, FG_COLOUR);
            draw_string(0, 20, "Score: ", FG_COLOUR);
            draw_double(30, 20, score, FG_COLOUR);
            show_screen();
            if ( (( c == 'p') || (BIT_VALUE ( PINB , 0 )))) {
                paused = false;
                CLEAR_BIT(DDRB, 0);
                difference = current_time() - timeOfPress;
                clear_screen();
            }
        } 
    }
CLEAR_BIT(DDRB, 0);
}



void blocks(void){
  	while(total_forbid < 3 && total_safe < 8){
        total_forbid = 0;
	    total_safe = 0;
		unsigned char *block_image;
		for( int i = 1; i < 21; i++ ){
			int z = rand() % 7;
			if ( z <= 5 && ((i % 5) != 0)){
				total_safe++;
				allBlocks[i] = true;
				block_image = blocks_safe;
				}
			else if ( z > 5 && ((i % 5) != 0)){
                    total_forbid++;
                    allBlocks[i] = false;
                    block_image = blocks_forbid;
			    }
            if ((i % 5) == 0){
                allBlocks[i] = true;
                sprite_init(&allBlocksSprite[i], -11, block_y, 10, 2, load_rom_bitmap(blocks_safe, 4));
                free(load_rom_bitmap(treasure, 4));
                block_y += 10;
                block_x = 10;
            } else {
                sprite_init(&allBlocksSprite[i], block_x, block_y, 10, 2, load_rom_bitmap(block_image, 4));
                free(load_rom_bitmap(block_image, 4));
                block_x = block_x + 21;
            }
		}
	}
}

void block_draw(){
    for (int i = 1; i < 21; i++){

        if (allBlocksSprite[i].x > 93)
        {
            allBlocksSprite[i].x = -10;
        }

        else if (allBlocksSprite[i].x < -19)
        {
            allBlocksSprite[i].x = 84;
        }

        else if (i <= 5){
            allBlocksSprite[i].dx = speed;
            sprite_step(&allBlocksSprite[i]);
            block_direction[i] = 0;
        }
        else if (i > 5  && i <= 10){
            allBlocksSprite[i].dx = -1*(speed);
            sprite_step(&allBlocksSprite[i]);
            block_direction[i] = 1;
        }
        else if (i > 10 && i <= 15){
            allBlocksSprite[i].dx = speed;
            sprite_step(&allBlocksSprite[i]);
            block_direction[i] = 0;
        }
        else if (i > 15 && i <= 20){
            allBlocksSprite[i].dx = -1 * (speed);
            sprite_step(&allBlocksSprite[i]);
            block_direction[i] = 1;
        }        
        sprite_draw(&allBlocksSprite[i]);
    }
}

void draw_treasure(){
    sprite_draw(&treasure_sprite);
    if(end == false){
        treasure_sprite.x += 2;
        treasure_dir = true;
		sprite_step(&treasure_sprite);
		if(treasure_sprite.x > 79){
			end = true;
			back = false;
		}
	} else if (end == true && back == false){
        treasure_sprite.x -= 2;
        treasure_dir = false;
		sprite_step(&treasure_sprite);
		if (treasure_sprite.x < 1){
			back = true;
			end = false;
		}
	} else if (end == true && back == true){
		treasure_sprite.x += 0;
	} 
    if ((c == 't') || (BIT_VALUE(PINF, 5))){
		if ( toggle == true ){
		end = true;
		back = true;
		toggle = false;
		} else {
            if (treasure_dir == true){
                end = false;
                toggle = true;
            } else {
                end = true; 
                back = false;
                toggle = true;
            }
		}
    }
}

int sprites_collide( Sprite s1, Sprite s2 ){
    int l1 = s1.x;
    int l2 = s2.x;
    int t1 = s1.y;
    int t2 = s2.y;
    int r1 = l1 + s1.width;
    int r2 = l2 + s2.width;
    int b1 = t1 + s1.height;
    int b2 = t2 + s2.height;
    if ( ( ( r1 >= l2 && r1 <= r2 ) || (l1 >= l2 && l1 <= r2 )) && b1 == t2){
		return 3;
	} else if ( t1 == b2 && l1 >= l2 && l1 <= r2 ){
		return 2;
	} else if (l1 == r2 && t1 == r2 && b1 == r2){
        player_direction = 1;
		return 1;
	} else if (r1 == l2 && t1 == l2 && b1 == l1){
        player_direction = 0;
		return 1;
	} else{
		return 0;
	}
}

int player_collide_block(){
    int result = 0;

    for ( int i = 0; i < 21; i++ )
    {
        if ( sprites_collide( player_sprite, allBlocksSprite[i] ) != 0 )
        {
            if (sprites_collide( player_sprite, allBlocksSprite[i]) == 3 && scored == false && allBlocks[i] == true){
                score++;
                scored = true;
            }
            result = sprites_collide( player_sprite, allBlocksSprite[i]);
            break;


        } else if (sprites_collide( player_sprite, allBlocksSprite[i] ) == 1){
            if(player_direction == 1){
                player_sprite.x += speed;
            } else if (player_direction == 0){
                player_sprite.x -= speed;
            }


        } else if (sprites_collide( player_sprite, allBlocksSprite[i] ) != 1){
            player_sprite.dx = 0;
        } else if (sprites_collide( player_sprite, allBlocksSprite[i] ) == 0){
            result = 0;
        }
    }
    return result;
}

void player_collide_treasure(){ 
    if (sprites_collide(player_sprite, treasure_sprite) > 0){
        lives += 2;
        sprite_init(&treasure_sprite, 0, 60, 3, 3, load_rom_bitmap(treasure, 3));
        free(load_rom_bitmap(treasure, 3));
        treasure_sprite.is_visible = 0;
        treasure_message = true;
        sprite_init(&player_sprite, spawn_x, spawn_y, 3, 4, load_rom_bitmap(player, 4));
        free(load_rom_bitmap(player, 4));
    }
}

void gravity(){
if (player_collide_block() < 3)
    {
        if (player_sprite.y < 49){
            player_sprite.y += 1;
        } 
    }
else if ( player_collide_block() == 3 )
    {
        for(int i = 1; i < 21; i++){
            if(block_direction[i] == 1){
                player_sprite.dx = speed;
            }
            else if (block_direction[i] == 0){
                player_sprite.dx = speed;
            }
        }
        
        player_sprite.y = player_sprite.y;
    }
    sprite_step(&player_sprite);
}

bool bad_check(){
    for(int i = 0; i < 21; i++){
        if (sprites_collide(player_sprite, allBlocksSprite[i])){
            if (allBlocks[i] == false){
                return true;
            } else {
                return false;
            }
        }
    }
}

void draw_player(){
    if ( player_sprite.y >= 49 || player_sprite.x > 84 || player_sprite.x < 0 || bad_check() == true) {
        sprite_init(&player_sprite, spawn_x, spawn_y, 3, 4, load_rom_bitmap(player, 4));
        free(load_rom_bitmap(player, 4));
        if(spawn == false){
            lives--;
        }
        spawn = true;
    }
    if (((c == 'a') || (BIT_IS_SET(PINB, 1))) && player_collide_block() == 3){
        player_sprite.x -= 2;
        sprite_step(&player_sprite);
    } else if (((c == 'd') || (BIT_IS_SET(PIND, 0))) && player_collide_block() == 3){
        player_sprite.x += 2;
        sprite_step(&player_sprite);
    } else if (((c == 'w') || (BIT_IS_SET(PIND, 1))) && (current_time() > timeOfPress_up + 1) && player_collide_block() == 3){
        timeOfPress_up = current_time();
        if (player_collide_block() != 2){
            for (int i = 0; i < 13; i++){
                scored = false;
                player_sprite.y -= 1;
                sprite_step(&player_sprite);
            }
        } else {
            player_sprite.y = player_sprite.y;
        }
    }
    sprite_draw(&player_sprite);
}

double play_time() {
    if ( paused == false ){
    return cur_time = current_time() - difference;
    }
}

void time_format(){
    mins = (cur_time/60);
    secs = cur_time - (mins*60); 
}

void setup_usb_serial(void) {
	// Set up LCD and display message
	lcd_init(LCD_LOW_CONTRAST);
	show_screen();

	usb_init();

	while ( !usb_configured() ) {
		// Block until USB is ready.
	}
}

void serial_check(){
    if (bad_check() == true){
        usb_serial_send("Event: Player Death by Bad Platform.\r\n");
        usb_serial_send("Lives Remaining: ");
        usb_serial_send_int(lives);
        usb_serial_send("\nScore: ");
        usb_serial_send_int(score);
        usb_serial_send("\nGame Time: ");
        usb_serial_send_int(mins);
        usb_serial_send(":");
        usb_serial_send_int(secs);
        usb_serial_send("\r\n");
    } else if (player_sprite.y >= 49 || player_sprite.x > 84 || player_sprite.x < 0){
        usb_serial_send("Event: Player Death by Going Out of Bounds.\r\n");
        usb_serial_send("Lives Remaining: ");
        usb_serial_send_int(lives);
        usb_serial_send("\nScore: ");
        usb_serial_send_int(score);
        usb_serial_send("\nGame Time: ");
        usb_serial_send_int(mins);
        usb_serial_send(":");
        usb_serial_send_int(secs);
        usb_serial_send("\r\n");
    } else if (spawn == true){
        spawn = false;
        usb_serial_send("Event: Respawn.\nPlayer Position: ");
        usb_serial_send_int(spawn_x);
        usb_serial_send(", ");
        usb_serial_send_int(spawn_y);
        usb_serial_send(".\r\n");
    } else if (treasure_message == true){
        treasure_message = false;
        usb_serial_send("Event: Player Collected Treasure.\r\n");
        usb_serial_send("Score: ");
        usb_serial_send_int(score);
        usb_serial_send("Lives Remaining: ");
        usb_serial_send_int(lives);
        usb_serial_send("\nGame Time: ");
        usb_serial_send_int(mins);
        usb_serial_send(":");
        usb_serial_send_int(secs);
        usb_serial_send("\r\n");
        usb_serial_send("Player Position: ");
        usb_serial_send_int(spawn_x);
        usb_serial_send(", ");
        usb_serial_send_int(spawn_y);
        usb_serial_send(".\r\n");
                
    } else if (paused_pressed == true){
        paused_pressed = false;
        usb_serial_send("Event: Paused.\r\n");
        usb_serial_send("Lives Remaining: ");
        usb_serial_send_int(lives);
        usb_serial_send("\nScore: ");
        usb_serial_send_int(score);
        usb_serial_send("\nGame Time: ");
        usb_serial_send_int(mins);
        usb_serial_send(":");
        usb_serial_send_int(secs);
        usb_serial_send("\r\n");
    } else if (game_over == true){
        game_over = false;
        usb_serial_send("Event: Game Over.\r\n");
        usb_serial_send("Lives Finished With: ");
        usb_serial_send_int(lives);
        usb_serial_send("\nScore Finished With: ");
        usb_serial_send_int(score);
        usb_serial_send("\nGame Time Finished With: ");
        usb_serial_send_int(mins);
        usb_serial_send(":");
        usb_serial_send_int(secs);
        usb_serial_send("\r\n");
    }
}


void setup(void)
{
    //setting clock speed
    set_clock_speed(CPU_8MHz);
    //

    setup_usb_serial();
    usb_serial_send("Event: Game Start. \nPlayer Position: ");
    usb_serial_send_int(spawn_x);
    usb_serial_send(", ");
    usb_serial_send_int(spawn_y);
    usb_serial_send(".\r\n");

    //initializing and clearing screen
    lcd_init(LCD_LOW_CONTRAST);
    lcd_clear();
    //

    enable_inputs();

    //setting up timers
    TCCR1A = 0b00000000;
    TCCR1B = 0b00000001;
    //

    //enabling timer overflow interrupt
    TIMSK1 = 1;
    //

    //turn on interrupts
    sei();
    //
    setup_pot();
    blocks();
    introScreen();
}

void life_check(){
    while (lives == 0){
        usb_check();
        clear_screen();
        game_over = true;
        draw_string(0, 0, "GAME OVER :(", FG_COLOUR);
        draw_string(0, 10, "Score: ", FG_COLOUR);
        draw_double(30, 10, score, FG_COLOUR);
        draw_string(0, 20, "Time: ", FG_COLOUR);
        draw_double(24, 20, mins, FG_COLOUR);
        draw_string(36, 20, ":", FG_COLOUR);
        draw_double(40, 20, secs, FG_COLOUR);
        show_screen();
        if ((BIT_VALUE(PINF, 6)) || (c == 'q')){
            clear_screen();
            while(1){
                draw_string(0,0, "N10170791", FG_COLOUR);
                show_screen();
            }
        } else if ((BIT_VALUE(PINF, 5)) || (c == 'r')){
            score = 0;
            cur_time = 0;
            mins = 0;
            secs = 0;
            lives = 10;
            sprite_init(&player_sprite, spawn_x, spawn_y, 3, 4, load_rom_bitmap(player, 4));
            free(load_rom_bitmap(player, 4));
            sprite_init(&treasure_sprite, 0, 45, 3, 3, load_rom_bitmap(treasure, 3));
            free(load_rom_bitmap(treasure, 3));
            treasure_sprite.is_visible = 1;
        }
    }
}


void process(void)
{
    time_format();
    gravity();
    block_speed();
    play_time();
    pauseMenu();
    block_draw();
    draw_player();
    draw_treasure();
    player_collide_block();
    player_collide_treasure();
    block_check();
    usb_check();
    life_check();
    serial_check();
}

int main(void) {
	setup();

	for ( ;; ) {
        clear_screen();
		process();
        show_screen();
        _delay_ms(100);
	}

    return 0;
}