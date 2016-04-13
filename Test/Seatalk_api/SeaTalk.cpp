#include "Arduino.h"
#include "SeaTalk.h"
#include <stdio.h>
#include <stdlib.h>

SeaTalk_API::SeaTalk_API()
{
}

//on a besoin d'un pointeur ver le port serie utilise pour envoyer les valeurs
//buffout est un buffer suffisament gros pour enregistrer tout les message qui on circuler avant l'envoi du message
int SeaTalk_API::send_bouton_value(HardwareSerial * serial_write, HardwareSerial * serial_read, int val, char buffout[])
{
	int i,j;
	//on test que le port soit libre avant d'envoyer le message
	//si message dans le buffer available != 0
	if((*serial_read).available() != 0)
	{
		unsigned int readchar = 0;
		//si il ne l'ai pas, on vide le cache de message et on attend 10/4800 s soi 2,08 millisecond
		//pour que le bus soit considÃƒÂ©rÃƒÂ© comme libre
		unsigned char libre = 0;
		while(!libre)
		{
			buffout[readchar] = (*serial_read).read(); //on lit un char du buffer pour le vider
			readchar++;
			//si plus de char dans le buffer, le bus est libre
			if((*serial_read).available() == 0)
			{
				//on attend 2ms pour que le bus soit considerer comme libre
				delay(2);
				//si le bus est encore libre, on peux commencer a emetre
				if((*serial_read).available() == 0)
				{
					libre = 1;
					buffout[readchar] = '\0';
				}
			}
		}
	}
	if(val < 0)
	{
		val = val * -1; //on convertie la valeur negative en positive
		for(i=0; i < (val/10); i++)
		{
			if(send_bouton_m10(serial_write, serial_read) == -1)
			{
				return i*10;
			}
		}
		for(j=0; j < (val%10); j++)
		{
			if(send_bouton_m1(serial_write, serial_read) == -1)
			{
				return (i*10) + j;
			}
		}
	}
	else
	{
		for(i=0; i < (val/10); i++)
		{
			if(send_bouton_p10(serial_write, serial_read) == -1)
      {
        return i*10;
      }
		}
		for(j=0; j < (val%10); j++)
		{
			if(send_bouton_p1(serial_write, serial_read) == -1)
     {
        return (i*10) + j;
     }
		}
	}
}

//-1
int SeaTalk_API::send_bouton_m1(HardwareSerial * serial_write, HardwareSerial * serial_read)
{
	uint16_t c[] = {0x86,0x11, 0x05, 0xFA};
	volatile int i = 0;
	//on envoi les 4 char de la trame
	for(i=0; i< 4; i++)
	{
		serial_write->write9(c[i] , ( i == 0));
	}
	//on attend de reÃƒÂ§evoire la trame que l'on a envoyer.
	while(!serial_read->available()){};
	for(i = 0; i < 4; i++)
	{
		volatile unsigned char r = serial_read->read();
		if( r != c[i])
		{
			return -1;
		}
	}
	return 0;
	
}
//-10
int SeaTalk_API::send_bouton_m10(HardwareSerial * serial_write, HardwareSerial * serial_read)
{
	uint16_t c[] = {0x86,0x11, 0x06, 0xF9};
	volatile int i = 0;
	//on envoi les 4 char de la trame
	for(i=0; i< 4; i++)
	{
		serial_write->write9(c[i] , ( i == 0));
	}
	//on attend de reÃƒÂ§evoire la trame que l'on a envoyer.
	while(!serial_read->available()){};
	for(i = 0; i < 4; i++)
	{
		volatile unsigned char r = serial_read->read();
		if( r != c[i])
		{
			return -1;
		}
	}
	return 0;
}
//+1
int SeaTalk_API::send_bouton_p1(HardwareSerial * serial_write, HardwareSerial * serial_read)
{
	uint16_t c[] = {0x86,0x11, 0x07, 0xF8};
	volatile int i = 0;
	//on envoi les 4 char de la trame
	for(i=0; i< 4; i++)
	{
		serial_write->write9(c[i] , ( i == 0));
	}
	//on attend de reÃƒÂ§evoire la trame que l'on a envoyer.
	while(!serial_read->available()){};
	for(i = 0; i < 4; i++)
	{
		volatile unsigned char r = serial_read->read();
		if( r != c[i])
		{
			return -1;
		}
	}
	return 0;
}
//+10
int SeaTalk_API::send_bouton_p10(HardwareSerial * serial_write, HardwareSerial * serial_read)
{	
	uint16_t c[] = {0x86,0x11, 0x08, 0xF7};
	volatile int i = 0;
	//on envoi les 4 char de la trame
	for(i=0; i< 4; i++)
	{
		serial_write->write9(c[i] , ( i == 0));
	}
	//on attend de reÃƒÂ§evoire la trame que l'on a envoyer.
	while(!serial_read->available()){};
	for(i = 0; i < 4; i++)
	{
		volatile unsigned char r = serial_read->read();
		if( r != c[i])
		{
			return -1;
		}
	}
}


//9C  U1  VW  RR    Compass heading and Rudder position 
//trame pour la direction du compas et de la barre, 9c id du message uvw pour la direction, 1 octet suplementaire, rr pour la barre
void SeaTalk_API::send_heading_rudder(HardwareSerial * serial_write, HardwareSerial * serial_read, int heading, int rudder)
{
	uint16_t c;
	uint16_t vw, u_low, u_hight;
	u_low = heading/90; 
	vw = (heading%90)/2;
	u_hight = (heading%90)%2;
	
	c = 0x9C;
	(*serial_write).write9(c ,true);
	
	c = ((u_hight<<2 + u_low) << 4) + 0x01;
	(*serial_write).write9(c ,false);
	
	(*serial_write).write9(vw , false);
	//
	(*serial_write).write9((uint16_t)rudder & 0x00FF ,false);
}

//si la trame emise par le bus seatalk correspond a "9C  U1  VW  RR" ou "84  U6  VW  XY 0Z 0M RR SS TT"
//cette fonction permet de convertir les valeur recus par le bus seatalk en entier.
void SeaTalk_API::read_seatalk_heading_rudder(char * buff, boolean parsed, int* heading, int* rudder)
{
	//la version precedente ne pouvait pas fonctionner, ce n'est pas une chaine de caractere mais des valeur hexadecimal qu'il faut oarser
	//la trame traduit par "9C U1 VW RR" ÃƒÂ  l'affichage correspond a "0x9C 0xU1 0xVW 0xRR" sans espace
	//il faut donc lire le premier char pour avoir la comande le deuxieme char pour avoir la taille ...
	//la chaine ÃƒÂ©tant stocke dans un char, le 9eme bit a ete tronque la commande n'a plus le 1 la valeur n'est donc plus par ex 0x19C mais 0x9C
	int u_low, u_hight , vw;
	if(buff[0] == 0x9C)
	{//"9C  U1  VW  RR"
		u_low = (buff[1] & 0x30) >> 4; // U correspond au 4 bits de poid fort du 2eme char, u_low correspond donc au 2 premier bit de u
		u_hight = (buff[1] & 0xC0) >> 4; //la partie haute contien l'indication de direction, le msb, et le nombre bit a 1 est utiliser pour coder la valeur de heading
		vw = buff[2];
		*rudder = buff[3]; //a tester, la valeur etant en complement a 2, a voir comment est gerer la difference de precision la valeur  etant negative pour une barre a gauche
		*heading = u_low * 90 + vw * 2 +  ( u_hight == 0 ? ( u_hight == 0x0C ? 2 : 1) :0);
	}
	else
	{
		if(buff[0] == 0x84)
		{ // "84  U6  VW  XY 0Z 0M RR SS TT"
			u_low = (buff[1] & 0x30) >> 4;
			u_hight = (buff[1] & 0xC0) >> 4;
			vw = buff[2];
			*rudder = buff[6];
			*heading = u_low * 90 + vw * 2 +  ( u_hight == 0 ? ( u_hight == 0x0C ? 2 : 1) :0);
		}
	}
}

//on recupere une trame emise par le bus serie qui contient 2 entier au format "9C 125 -2"
//cette fonction permet de convertir la chaine e caractere recus par le bus serie en entier
void SeaTalk_API::read_serial_heading_rudder(char * buff, int* heading, int* rudder)
{
	char parsed[3][5];
	unsigned char buff_offset = 0, parsed_case = 0, parsed_offset = 0;
	
	//on rÃ¯Â¿Â½cupere chaque donnÃ¯Â¿Â½ de la chaine de charactere dans une chaine diffÃ¯Â¿Â½rente pour pouvoir convertire les donnÃ¯Â¿Â½s
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

void SeaTalk_API::read_seatalk_input(HardwareSerial * serial_read,unsigned char * buff,HardwareSerial * debug)
{
	unsigned int i = 0;
	unsigned int nb_char = 3; 
	//tant que l'on a des char a lire ou retounre une fois que l'on a lue une commande
	while(((*serial_read).available()) > 0)
	{
		uint16_t c;
		c =(*serial_read).read();
		//si l'on detecte une commande
		//si superieur a 0x200, on a des parasites ou un nombre negatif
		if(c > 0x100 && c < 0x200)
		{
			buff[0] = (unsigned char) c;
     
      (*debug).print(c, HEX);
      (*debug).print('_');
      (*debug).print(buff[0] , HEX);
			i = 1;
		}
	   else
	   {
			//si l'on a commence a ecrire une trame
			if(i > 0 && c < 0x100)
			{
				if(i == 1)
				{
					//4 lower bit => nb additional bit
					nb_char += c & 0x0000F;
				}
				buff[i] = (unsigned char) c;
       
      (*debug).print(c, HEX);
      (*debug).print('_');
      (*debug).print(buff[i] , HEX);
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
  buff[i] = '\0';
}


