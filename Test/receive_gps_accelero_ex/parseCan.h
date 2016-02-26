/**
	Romain Le Forestier
 decoupe les entiers et les flottants en tableau de char et inversement
permet d'envoyer des donner numérique sur le bus can 
*/

//fonction pour decouper les messages CAN
//basée sur le code de http://savvymicrocontrollersolutions.com/arduino.php?article=adafruit-ultimate-gps-shield-seeedstudio-can-bus-shield

#ifndef _PARSECAN_
#define _PARSECAN_

//liste des identifiant can
//on peut donner des identifiants plus grand pour les tram de données
//ainsi les tram qui envoi des commande seront prioritaire sur le bus

//Tram Gps
#define MSG_GPRMC_LAT_LONG		0x40 //identifiant pour une tram avec la latitude et la longitude
#define MSG_GPRMC_VIT_DATE		0x41 //identifiant pour une trame avec la vitesse et la date_order
#define MSG_GPGGA_ALT_PREC		0x42 //identifiant pour une trame avec l'altitude et la precision

//Tram IMU (accelerometre)
#define MSG_IMU_PHI_THETA_PSI 	0x50 //identifiant pour une trame avec Roll, pitch and yaw
#define MSG_GYRO_X_Y_Z 			0x51 //identifiant avec angular rates relative to the axes X, Y and Z, respectively. 

//Tram Seatalk
#define MSG_SETALK_BOUTON		0x30 //Identifiant pour une tram contenant une valeur pour un bouton

class ParseCan
{
    private:
        bool init;
	public:
	
		int int_gyro[3];
		unsigned char byte_gyro[6];
		
		unsigned char byte_seatalkButton[2];
		
        ParseCan(bool val);//inutile, sert a compiler
		//convertie 2 char d'un tableau en int
		int ucharToInt(unsigned char buff[], int offset);
		//convertie 4 char
		float ucharToFloat(unsigned char buff[], int offset);
		//buff tableau de char
		// offset position du msb

		//convertie un int en 2 char
		void intToUChar(unsigned char buff[], int offset, int val);
		//convertie un float en 4 char
		void floatToUChar(unsigned char buff[], int offset, float val);
		
		//donne du gyroscope
		int get_int_GYRO_X();
		int get_int_GYRO_Y();
		int get_int_GYRO_Z();
		
		//recupere la valeur du gyro a partir des donnees du bus can
		void set_int_GYRO(unsigned char buff[]);
		//convertie les entier en valeur pour le bus CAN
		void set_byte_GYRO(int x, int y, int z);
		
		//recupere la valur du bouton a partir des donnees du bus can
		int get_seatalk_bouton_value(unsigned char buff[]);
		//convertie l'entier correpondant a un bouton pour l'envoyer sur le bus can
		void set_seatalk_bouton_value(int value);
};

#endif
