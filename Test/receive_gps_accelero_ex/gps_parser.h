/**
	Romain Le Forestier
 parser pour nema gps data
*/

#ifndef GPS_PARSER_h
#define GPS_PARSER_h

#include <Arduino.h>

#define NMEALenght 80 //nmea by design is 80 char max per line

/*
* $GPRMC,225446,A,4916.45,N,12311.12,W,000.5,054.7,191194,020.3,E*68
field Name -> 0 -> $GPRMC
field Heure Fixe -> 1 -> 225446 -> 22H 54mn 46s
field gps signal valide -> 2 -> A -> A = ok | V = Warning
field Latitude -> 3 -> 4916.45 -> 49 deg. 16.45 min
field -> 4 -> N -> N = north
field lognitude -> 5 -> 12311.12 -> 123 deg. 11.12 min
field -> 6 -> W -> W = west
field vitesse -> 7 -> 000.5 -> 000.5 = vitesse sol, Knots
field cap -> 8 -> 054.7 -> cap (vrai)
field date fixe -> 9 -> 191194 -> Date du fix 19 Novembre 1994
field -> 10 -> 020.3 -> Déclinaison Magnetique 20.3 deg Est
field cheksum -> 11 -> E*68 -> cheksum 68
*/
typedef struct {
  char valide; 
  //latitude data
  char latInd;     //indicateur de latitude N=nord, S=sud
  char latDeg[3] = "00";  //degree from 0 to 90
  char latMn[8] = "0000000";   //minute
  
  //longitude data
  char longInd;  //indicateur de longitude E=est, W=ouest
  char longDeg[4] = "000";  //degree from 0 to 180
  char longMn[8] = "0000000";   //minute
  
  //date
  char day[3] = "00";
  char month[3] = "00";
  char year[3] = "00"; //seule les 2 dernier chiffres de l'annee
  
  //time utc
  char hour[3] = "00";
  char minute[3] = "00";
  char second[3] = "00";
  
  char speed[6] = "00000"; //vitesse en noeud
  }GPRMC_frame;
  
  //valeur a 0 par defaut au cas ou la trame est non valide
typedef struct {
  bool valide; 
  //latitude data
  float latitude=0.0;
  
  //longitude data
  float longitude=0.0;
  
  //date
  unsigned char day = 0;
  unsigned char month = 0;
  unsigned char year = 0;
  
  //time utc
  unsigned char hour= 0;
  unsigned char minute = 0;
  unsigned char second= 0;
  
  float speed = 0; //vitesse en noeud
  }GPRMC_data;
  
/*
*  $GPGGA,123519,4807.038,N,01131.324,E,1,08,0.9,545.4,M,46.9,M, , *42
*
* 01 -> 123519 = Acquisition du FIX à 12:35:19 UTC
* 02 -> 4807.038 = Latitude 48 deg 07.038'
* 03 -> N = north
* 04 -> 01131.324 = Longitude 11 deg 31.324'
* 05 -> E = East
* 06 -> 1 = Fix qualification : (0 = non valide, 1 = Fix GPS, 2 = Fix DGPS)
* 07 -> 08 = Nombre de satellites en pousuite.
* 08 -> 0.9 = Précision horizontale ou DOP (Horizontal dilution of position) Dilution horizontale.
* 09 -> 545.4= Altitude, au dessus du MSL (mean see level) niveau moyen des Océans.
* 10 -> M = en metre
* 11 -> 46.9 = Correction de la hauteur de la géoïde en Metres par raport à l'ellipsoîde WGS84 (MSL).
* 12 -> M = en metre
* 13 -> (Champ vide) = nombre de secondes écoulées depuis la dernière mise à jour DGPS.
* 14 -> (Champ vide) = Identification de la station DGPS.
* 15 -> 42 = Checksum
*  Non représentés CR et LF.
*/
  typedef struct {
  char valide; 
  //latitude data
  char latInd;     //indicateur de latitude N=nord, S=sud
  char latDeg[3] = "00";  //degree from 0 to 90
  char latMn[8] = "0000000";   //minute
  
  //longitude data
  char longInd;  //indicateur de longitude E=est, W=ouest
  char longDeg[4] = "000";  //degree from 0 to 180
  char longMn[8] = "0000000";   //minute
  
  //time utc
  char hour[3] = "00";
  char minute[3] = "00";
  char second[3] = "00";
  
  char nbSat[3] = "00"; //Satellites are in view
  char accuracy[4] = "000"; //Relative accuracy of horizontal position
  
  char altitude[6] = "00000"; // altitude above mean sea level
  char altitudeUnite; // altitude unite mesure (meter)
  
  }GPGGA_frame;


class GPS_PARSER
{
	public:
        GPS_PARSER(boolean init);
		void parseGPRMC(const char buffer[], GPRMC_frame *data); //parse a gprmc sentence
		void parseGPGGA(const char buffer[], GPGGA_frame *data);	//parse a gpgga sentence
		void getNemaField(const char buffIn[], char field[], const unsigned char fieldNb); // get the field number from NMEA sentence
		boolean isGPRMC(const char buffer[]);
		boolean isGPGGA(const char buffer[]);
		
		void convertGprmcFrame(GPRMC_frame *data, GPRMC_data *val);
		
        private:
                boolean init;
};

#endif
