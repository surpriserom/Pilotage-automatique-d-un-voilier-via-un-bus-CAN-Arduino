/**
*	Romain Le Forestier
*	Seatalk - arduino
**/


#ifndef _SEATALKAPI_
#define _SEATALKAPI_

#include <HardwareSerial.h>

#define SeaTalk_Heading_Rudder 0x9C //identifiant d'une trame Serial pour le heading et le rudder
#define SeaTalk_Autopilote_Heading_Rudder 0x84

class SeaTalk_API
{
	public:
    SeaTalk_API();
		//on a besoin d'un pointeur ver le port serie utilise pour envoyer les valeurs
		void send_bouton_value(HardwareSerial * serial_write, HardwareSerial * serial_read, int val);
		//-1
		void send_bouton_m1(HardwareSerial * serial_write, HardwareSerial * serial_read);
		//-10
		void send_bouton_m10(HardwareSerial * serial_write, HardwareSerial * serial_read);
		//+1
		void send_bouton_p1(HardwareSerial * serial_write, HardwareSerial * serial_read);
		//+10
		void send_bouton_p10(HardwareSerial * serial_write, HardwareSerial * serial_read);
		
		void send_heading_rudder(HardwareSerial * serial_write, HardwareSerial * serial_read, int heading, int rudder);
		void read_seatalk_heading_rudder(char buff[], int* heading, int* rudder);
		void read_serial_heading_rudder(char buff[], int* heading, int* rudder);
};

#endif
