/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package javacommunicationarduino;

import com.embeddedunveiled.serial.ISerialComDataListener;
import com.embeddedunveiled.serial.SerialComDataEvent;

/**
 *
 * @author romain
 */

class DataListener implements ISerialComDataListener
{
	@Override
	public void onNewSerialDataAvailable(SerialComDataEvent data)
        {
		System.out.println("Read from serial port : " + new String(data.getDataBytes()) + "\n");
	}

	@Override
	public void onDataListenerError(int arg0) {
		System.out.println("onDataListenerError called");
	}
}