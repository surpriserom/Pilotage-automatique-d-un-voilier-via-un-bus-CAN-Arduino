#include "mcp_can.h"
#include <SPI.h>
#include "gps_parser.h"
#include "parseCan.h"

// the cs pin of the version after v1.1 is default to D9
// v0.9b and v1.0 is default D10
const int SPI_CS_PIN = 9;
const int led = 13;
boolean state = false;

const int sentenceSize = 80;

unsigned char staticGPRMC[] = "$GPRMC,023934.00,A,4823.97002,N,00430.49225,W,0.636,,120615,,,D*64";

unsigned char sentence[sentenceSize];

GPS_PARSER parserGps(true);
ParseCan parserCan(true);

MCP_CAN CAN(SPI_CS_PIN); // Set CS pin

void setup()
{
  /*
    Serial.begin(9600);
    
    while (!Serial) {
        ; // wait for Serial1 port to connect. Needed for Leonardo only
    }
    */
    Serial1.begin(9600);
    
    while (!Serial1) {
        ; // wait for Serial1 port to connect. Needed for Leonardo only
    }
    
  //set the test signal led
  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);
  
START_INIT:

    if(CAN_OK == CAN.begin(CAN_500KBPS))                   // init can bus : baudrate = 500k
    {
        //Serial.println("CAN BUS Shield init ok!");
        digitalWrite(led, HIGH);
    }
    else
    {
        //Serial.println("CAN BUS Shield init fail");
        //Serial.println("Init CAN BUS Shield again");
        state = !state;
        digitalWrite(led, (state ? HIGH : LOW));
        delay(150);
        goto START_INIT;
    }
}

void loop()
{
    static unsigned long time1 = 0;
    static unsigned long time2 = 0;
    volatile unsigned long time3 = millis();
    static int i = 0;
    
    //if (Serial1.available()>0) //commenter car on utilise une trame deja enregistrer
    if(time3 > time2)
    {
      time2 += 500;
         GPRMC_frame frame;
          GPRMC_data data;
	  unsigned char buff1[8];
	  unsigned char buff2[8];
          
         //  parserGps.parseGPRMC((char *) sentence, &frame);
         parserGps.parseGPRMC((char *) staticGPRMC, &frame);
		 parserGps.convertGprmcFrame(&frame, &data);
          if(frame.valide == 'A')
          {
		 parserCan.floatToUChar(buff1,0,data.latitude);
		 parserCan.floatToUChar(buff1,4,data.longitude);
		 CAN.sendMsgBuf(MSG_GPRMC_LAT_LONG, 0, 8, buff1);
		 parserCan.floatToUChar(buff2,0,data.speed);
		 buff2[4] = data.day;
		 buff2[5] = data.month;
		 buff2[6] = data.year;
		 CAN.sendMsgBuf(MSG_GPRMC_VIT_DATE, 0, 8, buff2);
          } 
          else
          {
            //Serial.println("mauvais signal");
          }
    }
          
    if(time3 > time1 )
    {
      state = !state;
      digitalWrite(led, (state ? HIGH : LOW));
      time1 = time1 + (state ? 500 : 1000);
    }
}

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/
