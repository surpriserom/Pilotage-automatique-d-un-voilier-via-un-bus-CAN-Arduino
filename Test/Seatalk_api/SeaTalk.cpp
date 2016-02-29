

#include "SeaTalk.h"


//on a besoin d'un pointeur ver le port serie utilise pour envoyer les valeurs
void SeaTalk_API::send_bouton_value(HardwareSerial * serial_write, HardwareSerial * serial_read, int val)
{
	int i;
	if(val < 0)
	{
		val = val * -1; //on convertie la valeur negative en positive
		for(i=0; i < (val/10); i++)
		{
			send_bouton_m10(serial_write, serial_read);
		}
		for(i=0; i < (val%10); i++)
		{
			send_bouton_m1(serial_write, serial_read);
		}
	}
	else
	{
		for(i=0; i < (val/10); i++)
		{
			send_bouton_p10(serial_write, serial_read);
		}
		for(i=0; i < (val%10); i++)
		{
			send_bouton_p1(serial_write, serial_read);
		}
	}
}

//-1
void SeaTalk_API::send_bouton_m1(HardwareSerial * serial_write, HardwareSerial * serial_read)
{
	uint16_t c;
	//on verifie qu'aucune donné n'est en attente
	//if(serial_read.available() == 0) // probleme de ce tese est qu'il faille vidé la buffer -> que fait on des donnés... si elle ne sont pas lut risque de débordement du buffer de 64Bytes
	c = 0x86;
	serial_write.write9(c ,true);
	c = 0x11;
	serial_write.write9(c ,false);
	c = 0x05;
	serial_write.write9(c ,false);
	c = 0xFA;
	serial_write.write9(c ,false);
}
//-10
void SeaTalk_API::send_bouton_m10(HardwareSerial * serial_write, HardwareSerial * serial_read)
{
	uint16_t c;
	//on verifie qu'aucune donné n'est en attente
	//if(serial_read.available() == 0)
	c = 0x86;
	serial_write.write9(c ,true);
	c = 0x11;
	serial_write.write9(c ,false);
	c = 0x06;
	serial_write.write9(c ,false);
	c = 0xF9;
	serial_write.write9(c ,false);
}
//+1
void SeaTalk_API::send_bouton_p1(HardwareSerial * serial_write, HardwareSerial * serial_read)
{
	uint16_t c;
	//on verifie qu'aucune donné n'est en attente
	//if(serial_read.available() == 0)
	c = 0x86;
	serial_write.write9(c ,true);
	c = 0x11;
	serial_write.write9(c ,false);
	c = 0x07;
	serial_write.write9(c ,false);
	c = 0xF8;
	serial_write.write9(c ,false);
}
//+10
void SeaTalk_API::send_bouton_p10(HardwareSerial * serial_write, HardwareSerial * serial_read)
{
	uint16_t c;
	//on verifie qu'aucune donné n'est en attente
	//if(serial_read.available() == 0)
	c = 0x86;
	serial_write.write9(c ,true);
	c = 0x11;
	serial_write.write9(c ,false);
	c = 0x08;
	serial_write.write9(c ,false);
	c = 0xF7;
	serial_write.write9(c ,false);
}

void SeaTalk_API::send_heading_rudder(HardwareSerial * serial_write, HardwareSerial * serial_read, int heading, int rudder)
{
	uint16_t c;
	uint16_t vw;
	c = 0x9C;
	
	
}
