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
		//on a besoin d'un pointeur ver le port serie utilise pour envoyer les valeurs, retourne la valeur envoyer,
		//si la valeur de retour est diffÃ¯Â¿Â½rente de la valeur envoyer, c'est qu'il y a eu une erreur de transmition
		int send_bouton_value(HardwareSerial * serial_write, HardwareSerial * serial_read, int val, char buffout[]);
		//-1
		int send_bouton_m1(HardwareSerial * serial_write, HardwareSerial * serial_read);
		//-10
		int send_bouton_m10(HardwareSerial * serial_write, HardwareSerial * serial_read);
		//+1
		int send_bouton_p1(HardwareSerial * serial_write, HardwareSerial * serial_read);
		//+10
		int send_bouton_p10(HardwareSerial * serial_write, HardwareSerial * serial_read);
		
		void send_heading_rudder(HardwareSerial * serial_write, HardwareSerial * serial_read, int heading, int rudder);
		void read_seatalk_heading_rudder(char * buff, boolean parsed, int* heading, int* rudder);
		void read_serial_heading_rudder(char * buff, int* heading, int* rudder);
		//char we gonna loose 9bits, but not needed, we put in buf only command,
		//buff have to be big enought to store a long seatalk tram => 22 char 3 mandatory and up to 18 more + end string char
		void read_seatalk_input(HardwareSerial * serial_read,unsigned char buff[],HardwareSerial * debug); 
};

#endif


