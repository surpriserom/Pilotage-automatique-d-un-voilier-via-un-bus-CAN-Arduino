/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package javacommunicationarduino;

import com.embeddedunveiled.serial.ISerialComDataListener;
import com.embeddedunveiled.serial.SerialComDataEvent;

/**
 * Interface pour gérer un listener sur un port serie
 * Permet de détecter quand des donné sont disponible et les erreurs
 * @author romain
 */
class DataListener implements ISerialComDataListener
{
    /**
     * Fonction appeler quand une donné est disponible sur 
     * le port serie pour laquel le listener est enregistré
     * @param data 
     */
    @Override
    public void onNewSerialDataAvailable(SerialComDataEvent data)
    {
	System.out.println("Read from serial port : " + new String(data.getDataBytes()) + "\n");
    }

    /**
     * Fonction appeler lorsqu'une erreur arrive sur le listener
     * @param arg0 
     */
    @Override
    public void onDataListenerError(int arg0)
    {
        System.out.println("onDataListenerError called");
    }
}