/**
*	Romain Le Forestier
*	Seatalk - arduino
**/


#ifndef _SEATALKAPI_
#define _SEATALKAPI_

#include <HardwareSerial.h>

class SeaTalk_API
{
	public:
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
}

#endif