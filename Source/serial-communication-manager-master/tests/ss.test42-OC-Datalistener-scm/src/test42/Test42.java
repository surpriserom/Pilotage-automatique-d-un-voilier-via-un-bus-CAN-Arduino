/**
 * Author : Rishi Gupta
 * 
 * This file is part of 'serial communication manager' library.
 *
 * The 'serial communication manager' is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by the Free Software 
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * The 'serial communication manager' is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
 * PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with serial communication manager. If not, see <http://www.gnu.org/licenses/>.
 */

package test42;

import java.util.concurrent.atomic.AtomicBoolean;

import com.embeddedunveiled.serial.ISerialComDataListener;
import com.embeddedunveiled.serial.SerialComDataEvent;
import com.embeddedunveiled.serial.SerialComManager;
import com.embeddedunveiled.serial.SerialComManager.BAUDRATE;
import com.embeddedunveiled.serial.SerialComManager.DATABITS;
import com.embeddedunveiled.serial.SerialComManager.FLOWCONTROL;
import com.embeddedunveiled.serial.SerialComManager.PARITY;
import com.embeddedunveiled.serial.SerialComManager.STOPBITS;

class DataListener extends Test42 implements ISerialComDataListener{

	int y = 0;
	
	@Override
	public void onNewSerialDataAvailable(SerialComDataEvent data) {
		byte[] buf = data.getDataBytes();
		System.out.println("DataListener : " + new String(buf));
		System.out.println("DataListener : " + buf.length);
		
		y = y + buf.length;
		if(y >= 5) {
			exit.set(true);
		}
	}
	@Override
	public void onDataListenerError(int arg0) {
		System.out.println("error : " + arg0);
	}
}

// whole cycle create instance of scm, open, configure, write, listener, close repeated many times.
public class Test42 {
	
	protected static AtomicBoolean exit = new AtomicBoolean(false);
	
	public static void main(String[] args) {
		
		SerialComManager scm = null;
		DataListener dataListener = null;
		
		int x = 0;
		for(x=0; x<5000; x++) {
			System.out.println("\n" + "Iteration : " + x);
			try {
				scm = new SerialComManager("/home/r/ws-host-uart", "mylib");
				dataListener = new DataListener();
	
				String PORT = null;
				String PORT1 = null;
				int osType = scm.getOSType();
				if(osType == SerialComManager.OS_LINUX) {
					PORT = "/dev/ttyUSB0";
					PORT1 = "/dev/ttyUSB1";
				}else if(osType == SerialComManager.OS_WINDOWS) {
					PORT = "COM51";
					PORT1 = "COM52";
				}else if(osType == SerialComManager.OS_MAC_OS_X) {
					PORT = "/dev/cu.usbserial-A70362A3";
					PORT1 = "/dev/cu.usbserial-A602RDCH";
				}else if(osType == SerialComManager.OS_SOLARIS) {
					PORT = null;
					PORT1 = null;
				}else{
				}

				long handle = scm.openComPort(PORT, true, true, true);
				scm.configureComPortData(handle, DATABITS.DB_8, STOPBITS.SB_1, PARITY.P_NONE, BAUDRATE.B115200, 0);
				scm.configureComPortControl(handle, FLOWCONTROL.NONE, 'x', 'x', false, false);
				long handle1 = scm.openComPort(PORT1, true, true, true);
				scm.configureComPortData(handle1, DATABITS.DB_8, STOPBITS.SB_1, PARITY.P_NONE, BAUDRATE.B115200, 0);
				scm.configureComPortControl(handle1, FLOWCONTROL.NONE, 'x', 'x', false, false);

				System.out.println("main thread register  : " + scm.registerDataListener(handle, dataListener));
				scm.writeString(handle1, "22222222222222", 0); // length of this string is 20
				
				// wait till data listener has received all the data
				while(exit.get() == false) { 
					scm.writeString(handle1, "22222222222", 0);
				}
				exit.set(false);                                     // reset flag

				System.out.println("main thread unregister : " + scm.unregisterDataListener(dataListener));
				
				scm.closeComPort(handle);
				scm.closeComPort(handle1);
			} catch (Exception e) {
				e.printStackTrace();
			}
		}
	}
}