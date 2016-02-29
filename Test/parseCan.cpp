//fonction pour decouper les messages CAN
//basée sur le code de http://savvymicrocontrollersolutions.com/arduino.php?article=adafruit-ultimate-gps-shield-seeedstudio-can-bus-shield
//un int est stocke pour la leonardo sur 16octets
//un float est stocke sur 32 octets

/*
pour verifier le decoupage on a tester le code suivant:
	volatile int nb = 100;
    volatile int nb2 = 5;
    
    Serial1.println(nb);
    test[0] = (nb>> 8) &0xff; 
    test[1] = nb & 0xff;
    nb2 = test[1] + ((int) test[0] << 8);
    Serial1.println(nb2);
*/

/*
pour tester les fonctions on teste le code suivant:
	float nb1 = 25.24;
    int nb2 = 245;
    unsigned char buff[6];
    Serial1.println(nb1);
    Serial1.println(nb2);
    test.intToUChar(buff,0,nb2);
    test.floatToUChar(buff,2,nb1);
    Serial1.print(buff[0], HEX);
    Serial1.print(' ');
    Serial1.print(buff[1], HEX);
    Serial1.print(' ');
    Serial1.print(buff[2], HEX);
    Serial1.print(' ');
    Serial1.print(buff[3], HEX);
    Serial1.print(' ');
    Serial1.print(buff[4], HEX);
    Serial1.print(' ');
    Serial1.println(buff[4], HEX);
    Serial1.println(test.ucharToFloat(buff,2));
    Serial1.println(test.ucharToInt(buff,0));
*/
#include "parseCan.h"

ParseCan::ParseCan(bool val)
{
  init = val;
}

//convertie 2 char d'un tableau en entier
int ParseCan::ucharToInt(unsigned char buff[], int offset)
{
	int nb = ((int) buff[offset] << 8) + buff[offset+1];
	return nb;
}

//convertie 4 char d'un tableau en float
float ParseCan::ucharToFloat(unsigned char buff[], int offset)
{
	//le principeest que sur une carte arduino, un float est stocké sur 4 Byte
	//l'union va stocke le float au meme emplacment memoire qu'un tableau de 4 byte_gyro
	//ce qui permet de transferer le float sans convertion suplémentaire
    union {
      float a;
      unsigned char bytes[4];
    } thing;
    for(int i = 0; i< 4; i++)
    {
      thing.bytes[i] = buff[offset+i];
    }
    return thing.a;
}

//stocke les 2 octets d'un int dans 2 case d'un tableau à partire de offset
void ParseCan::intToUChar(unsigned char buff[], int offset, int val)
{
	buff[offset] = (val >> 8) & 0xff;
	buff[offset+1] = val & 0xff;
}

//stocke les 4 octets d'un float dans 4 case d'un tableaux
void ParseCan::floatToUChar(unsigned char buff[], int offset, float val)
{
  union {
    float a;
    unsigned char bytes[4];
  } thing;
  thing.a = val;
  for(int i = 0; i< 4; i++)
  {
    buff[offset+i] = thing.bytes[i];
  }
}

//donne du gyroscope
int ParseCan::get_int_GYRO_X()
{
	return int_gyro[0];
}

int ParseCan::get_int_GYRO_Y()
{
	return int_gyro[1];
}

int ParseCan::get_int_GYRO_Z()
{
	return int_gyro[2];
}
		
//recupere la valeur du gyro a partir des donnees du bus can
void ParseCan::set_int_GYRO(unsigned char buff[])
{
	int i;
	for(i = 0; i < 3; i++)
	{
		int_gyro[i] = ucharToInt(buff, i * 2);
		byte_gyro[i * 2] = buff[i*2];
		byte_gyro[(i * 2) + 1] = buff[(i*2) + 1];
	}
}

//convertie les entier en valeur pour le bus CAN
void ParseCan::set_byte_GYRO(int x, int y, int z)
{
	int_gyro[0] = x;
	intToUChar(byte_gyro,0,x);
	int_gyro[1] = y;
	intToUChar(byte_gyro,1,y);
	int_gyro[2] = z;
	intToUChar(byte_gyro,2,z);
}

//l'entier peux ne pas correspondre a une touche la conversion sera faite par la carte gérant la connection seatalk
//recupere la valur du bouton a partir des donnees du bus can
int ParseCan::get_seatalk_bouton_value(unsigned char buff[])
{
	return ucharToInt(buff, 0);
}

//convertie l'entier  pour l'envoyer sur le bus can
void ParseCan::set_seatalk_bouton_value(int value)
{
	intToUChar(byte_seatalkButton, 0, value);
}


void ParseCan::set_seatalk_heading_rudder(unsigned char buff[], int heading, int rudder)
{
	intToUChar(buff,0,heading);
	intToUChar(buff,2,rudder);
}

void ParseCan::get_seatalk_heading_rudder(unsigned char buff[], int* heading, int* rudder)
{
	*heading = ucharToInt(buff, 0);
	*rudder = ucharToInt(buff, 2);
}