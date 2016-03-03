

#include "SeaTalk.h"
#include <stdio.h>
#include <stdlib.h>

SeaTalk_API::SeaTalk_API()
{
}

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
	//on verifie qu'aucune donn� n'est en attente
	//if(serial_read.available() == 0) // probleme de ce tese est qu'il faille vid� la buffer -> que fait on des donn�s... si elle ne sont pas lut risque de d�bordement du buffer de 64Bytes
	c = 0x86;
	serial_write->write9(c ,true);
	c = 0x11;
	serial_write->write9(c ,false);
	c = 0x05;
	serial_write->write9(c ,false);
	c = 0xFA;
	serial_write->write9(c ,false);
}
//-10
void SeaTalk_API::send_bouton_m10(HardwareSerial * serial_write, HardwareSerial * serial_read)
{
	uint16_t c;
	//on verifie qu'aucune donn� n'est en attente
	//if(serial_read.available() == 0)
	c = 0x86;
	serial_write->write9(c ,true);
	c = 0x11;
	serial_write->write9(c ,false);
	c = 0x06;
	serial_write->write9(c ,false);
	c = 0xF9;
	serial_write->write9(c ,false);
}
//+1
void SeaTalk_API::send_bouton_p1(HardwareSerial * serial_write, HardwareSerial * serial_read)
{
	uint16_t c;
	//on verifie qu'aucune donn� n'est en attente
	//if(serial_read.available() == 0)
	c = 0x86;
	serial_write->write9(c ,true);
	c = 0x11;
	serial_write->write9(c ,false);
	c = 0x07;
	serial_write->write9(c ,false);
	c = 0xF8;
	serial_write->write9(c ,false);
}
//+10
void SeaTalk_API::send_bouton_p10(HardwareSerial * serial_write, HardwareSerial * serial_read)
{
	uint16_t c;
	//on verifie qu'aucune donn� n'est en attente
	//if(serial_read.available() == 0)
	c = 0x86;
	serial_write->write9(c ,true);
	c = 0x11;
	serial_write->write9(c ,false);
	c = 0x08;
	serial_write->write9(c ,false);
	c = 0xF7;
	serial_write->write9(c ,false);
}

void SeaTalk_API::send_heading_rudder(HardwareSerial * serial_write, HardwareSerial * serial_read, int heading, int rudder)
{
	uint16_t c;
	uint16_t vw, u_low, u_hight;
	u_low = heading/90; 
	vw = (heading%90)/2;
	u_hight = (heading%90)%2;
	
	c = 0x9C;
	(*serial_write).write9(c ,true);
	
	c = ((u_hight<<2 + u_low) << 4) + 0x06;
	(*serial_write).write9(c ,false);
	
	(*serial_write).write9(vw , false);
	//
	(*serial_write).write9((uint16_t)rudder & 0x00FF ,false);
}

//si la trame �mise par le bus seatalk correspond a "9C  U1  VW  RR" ou "84  U6  VW  XY 0Z 0M RR SS TT"
//cette fonction permet de convertir les valeur re�us par le bus seatalk en entier.
void SeaTalk_API::read_seatalk_heading_rudder(char buff[], boolean parsed, int* heading, int* rudder) // <- bugged char 8octet not 4 :/
{
	char parsed[7][5];
	unsigned char buff_offset = 0, parsed_case = 0, parsed_offset = 0;
	
	//on r�cupere chaque donn� de la chaine de charactere dans une chaine diff�rente pour pouvoir convertire les donn�s
	while(buff_offset < strlen(buff) && parsed_case < 7)
	{
		//si on detect un espace on change de tableau
		if(buff[buff_offset] == ' ')
		{
			//on insert le caractere de terminaiseon de chaine
			parsed[parsed_case][parsed_offset] = '\0';
			//on change de ligne de tableau et on remet l'offset du second tableau a 0
			parsed_case += 1;
			parsed_offset = 0;
		}
		else
		{
			parsed[parsed_case][parsed_offset] = buff[buff_offset];
			parsed_offset += 1;
		}
		buff_offset += 1;
	}
	//la derni�re chaine du tableau n'est pas terminer correctement
	parsed[parsed_case][parsed_offset] = '\0';
	
	//on teste si l'on � r�cup�r� la trame correspondant a SeaTalk_Heading_Rudder
	if(atoi(parsed[0]) == SeaTalk_Heading_Rudder)
	{
		uint16_t u, vw, rr;
		u = (atoi(parsed[1])>>4) & 0x0F;
		vw = atoi(parsed[2]);
		rr= atoi(parsed[3]);
		
		*heading = (u&0x03) * 90 + vw * 2 + (((u&0xC0)>>2)&0x03);
		*rudder = rr;
	}
	else
	{
		if(atoi(parsed[0]) == SeaTalk_Autopilote_Heading_Rudder)
		{
			uint16_t u, vw, rr;
			u = (atoi(parsed[1])>>4) & 0x0F;
			vw = atoi(parsed[2]);
			rr= atoi(parsed[6]);
			
			*heading = (u&0x03) * 90 + vw * 2 + (((u&0xC0)>>2)&0x03);
			*rudder = rr;
		}
	}
}

//on r�cupere une trame �mise par le bus s�rie qui contient 2 entier au format "9C 125 -2"
//cette fonction permet de convertir la chaine e caractere re�us par le bus s�rie en entier
void SeaTalk_API::read_serial_heading_rudder(char buff[], int* heading, int* rudder)
{
	char parsed[3][5];
	unsigned char buff_offset = 0, parsed_case = 0, parsed_offset = 0;
	
	//on r�cupere chaque donn� de la chaine de charactere dans une chaine diff�rente pour pouvoir convertire les donn�s
	while(buff_offset < strlen(buff) && parsed_case < 3)
	{
		//si on detect un espace on change de tableau
		if(buff[buff_offset] == ' ')
		{
			//on insert le caractere de terminaiseon de chaine
			parsed[parsed_case][parsed_offset] = '\0';
			//on change de ligne de tableau et on remet l'offset du second tableau a 0
			parsed_case += 1;
			parsed_offset = 0;
		}
		else
		{
			parsed[parsed_case][parsed_offset] = buff[buff_offset];
			parsed_offset += 1;
		}
		buff_offset += 1;
	}
	
	if(atoi(parsed[0]) == SeaTalk_Heading_Rudder)
	{
		*heading = atoi(parsed[1]);
		*rudder = atoi(parsed[2]);
	}
}

void SeaTalk_API::read_seatalk_input(HardwareSerial * serial_read, char buff[])
{
	unsigned int i = 0;
	unsigned int nb_char = 3; 
	//tant que l'on a des char a lire ou retunre une fois que l'on a lue une commande
	while((*serial_read).available())
	{
		uint16_t c = (*serial_read).read();
		//si l'on detect une commande
		if(c > 0x100)
		{
			buff[i] = (char) c;
			i++;
		}
		//si l'on a commencé a ecrire une trame
		if(i > 0)
		{
			if(i == 1)
			{
				//4 lower bit => nb additional bit
				nb_char += c & 0x0F;
			}
			buff[i] = c;
			i++;
			//si l'on a lut tous les charactere de la trame
			//on laisse le reste des charactere dans le buffer du bus serie de min 64char
			//et on retourne avec la trame lut
			if(i >= nb_char)
			{
				buff[i] = '\0';
				return;
			}
		}
	}
}