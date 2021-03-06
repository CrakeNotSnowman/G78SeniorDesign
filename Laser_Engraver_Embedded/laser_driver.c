//============================================================================
// Project	   : Laser Engraver Embedded
// Name        : laser_driver.c
// Author      : Garin Newcomb
// Email       : gpnewcomb@live.com
// Date		   : 2015-02-05 (Created), 2015-04-16 (Last Updated)
// Copyright   : Copyright 2014-2015 University of Nebraska-Lincoln
// Description : Source code to drive the analog input to the laser driver via
//				 PWM
//============================================================================


////////////////////////////////////////////////////////////////////////////////


#include "msp430f5529.h"
#include "defs.h"
#include "laser_driver.h"
#include "uart_fifo.h"
#include "time.h"
#include "motors.h"

////////////////////////////////////////////////////////////////////////////////


uint8_t laser_on = FALSE;
uint32_t intensity_buffer[100];
uint32_t x_pos_buffer[100];
uint32_t y_pos_buffer[750];

uint16_t buffer_it = 0;

extern volatile uint8_t burn_ready;
extern volatile uint8_t picture_ip;

////////////////////////////////////////////////////////////////////////////////


void init_laser( void )
{
	disable_laser();

	// Set up TimerA_0 for PWM on the laser input
	init_timer_A0();

	// Set Port 1.1 I/O to periphereal mode (i.e. Timer A0 output 1)
	P1DIR |= LASER_CTL_PIN;
	P1SEL |= LASER_CTL_PIN;


	// Set Port 1.2 I/O to GPIO mode (laser enable pin)
	P1OUT |=  LASER_ENA_PIN;	// Laser disabled
	P1SEL &= ~LASER_ENA_PIN;	// Select I/O
	P1DIR |=  LASER_ENA_PIN;	// Select output


	return;
}
//============================================================================



void disable_laser( void )
{
	P1OUT |= LASER_ENA_PIN;		// Laser enabled

	disable_fan();

	return;
}
//============================================================================



void enable_laser( void )
{
	P1OUT &= ~LASER_ENA_PIN;	// Laser disabled

	enable_fan();

	return;
}
//============================================================================



void init_fan( void )
{
	disable_fan();

	// Set Port 1.2 I/O to GPIO mode (laser enable pin)
	P7OUT &= ~FAN_ENA_PIN;	// Laser disabled
	P7SEL &= ~FAN_ENA_PIN;	// Select I/O
	P7DIR |=  FAN_ENA_PIN;	// Select output

	return;
}
//============================================================================



void enable_fan( void )
{
	P7OUT |= FAN_ENA_PIN;	// Fan enabled

	return;
}
//============================================================================



void disable_fan( void )
{
	P7OUT &= ~FAN_ENA_PIN;	// Fan disabled

	return;
}
//============================================================================



void turn_on_laser( uint16_t intensity )
{
	// Set Compare register 1 (Duty Cycle = TA0CCR1/TA0CCR0)
	TA0CCR1 = intensity;

	// Set output mode to 'Reset/Set' (OUTMOD = 111b)
	TA0CCTL1 |= OUTMOD2;
	TA0CCTL1 |= OUTMOD1;
	TA0CCTL1 |= OUTMOD0;
	
	laser_on = TRUE;

	return;
}
//============================================================================



void turn_on_laser_timed( uint16_t intensity, uint16_t duration )
{
	turn_on_laser( intensity );

	delay_ms( duration );

	turn_off_laser( );
	
	return;
}
//============================================================================



void turn_off_laser( void )
{
	// Set Compare register 1 to 0 (turns off laser)
	TA0CCR1 = 0;

	// Set output mode to 'OUT bit value' (OUTMOD = 000b)
	// Since the OUT bit value is always 0, this will turn it off
	TA0CCTL1 &= ~OUTMOD2;
	TA0CCTL1 &= ~OUTMOD1;
	TA0CCTL1 &= ~OUTMOD0;

	laser_on = FALSE;

	return;
}
//============================================================================



void respond_to_burn_cmd( uint8_t * burn_cmd_payload )
{
	// Perform a burn (move laser to position, turn on laser)
	uint32_t y_pos;
	uint32_t x_pos;
	uint32_t laser_intensity;
	volatile uint16_t temp0 = burn_cmd_payload[0];
	volatile uint16_t temp1 = burn_cmd_payload[1];
	volatile uint16_t temp2 = burn_cmd_payload[2];
	volatile uint16_t temp3 = burn_cmd_payload[3];



	parse_burn_cmd_payload( burn_cmd_payload,
							&y_pos,
							&x_pos,
							&laser_intensity );

	/*if( buffer_it < 750  )
	{
		intensity_buffer[buffer_it] = laser_intensity;
		x_pos_buffer[buffer_it] = x_pos;
		y_pos_buffer[buffer_it] = y_pos;
		buffer_it++;
	}*/
	//buffer_it++;
	

	// Move Laser
	if( moveMotors( x_pos, y_pos ) == 1 )
	{
		halt_burn();
	}

	
	if( !( P6IN & LID_OPEN ) )
	{
		disable_laser();
		while( !( P6IN & LID_OPEN ) );
		enable_laser();
	}


	switch( laser_intensity )
	{
		case 0:  turn_on_laser_timed( INTENSITY_1, LASER_DUR_1 );
				 break;

		case 1:  turn_on_laser_timed( INTENSITY_2, LASER_DUR_2 );
				 break;

		case 2:  turn_on_laser_timed( INTENSITY_3, LASER_DUR_3 );
				 break;

		case 3:  turn_on_laser_timed( MAX_INTENSITY, LASER_DUR_4 );
				 break;

		default: break;
	}


	// Reset the tracking variable
	burn_ready = FALSE;

	// When done executing the burn, request another command
	send_ready_for_pixel();

	return;
}
//============================================================================



void init_lid_safety( void )
{
	/////////////////////////// Sets up P6.4 as interrupt//////////////////
	P6DIR &= ~BIT4;  // pull up resistor
	P6REN |=  BIT4;  // pull up resistor
	P6OUT |=  BIT4;

	return;
}
//============================================================================



void halt_burn( void )
{
	// First disable the laser and make sure the PWM input is off
	disable_laser();
	turn_off_laser();
	
	picture_ip = FALSE;
	
	// Tell the Pi the burn is ending
	// send_burn_stop();
	
	return;
}

////////////////////////////////////////////////////////////////////////////////

/*
#pragma vector=PORT6_VECTOR
__interrupt void PORT_6_ISR(void)
{
	volatile uint16_t fuck_all_the_things = P2IV;

	if( fuck_all_the_things & P6IV_P6IFG2 ){   // Lid P2.2 interrupt
		lid = 0;

		debounce_lid = TRUE;

		// need to shut down laser here
		if( picture_ip == TRUE )
		{
			halt_burn();
		}
	}

	P2IFG &= ~BIT2; // P2.2 IFG cleared
}*/

////////////////////////////////////////////////////////////////////////////////


