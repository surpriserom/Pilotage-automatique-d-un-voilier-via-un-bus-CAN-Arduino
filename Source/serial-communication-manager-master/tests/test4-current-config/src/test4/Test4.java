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

package test4;

import java.util.Arrays;

import com.embeddedunveiled.serial.SerialComManager;
import com.embeddedunveiled.serial.SerialComManager.BAUDRATE;
import com.embeddedunveiled.serial.SerialComManager.DATABITS;
import com.embeddedunveiled.serial.SerialComManager.FLOWCONTROL;
import com.embeddedunveiled.serial.SerialComManager.PARITY;
import com.embeddedunveiled.serial.SerialComManager.STOPBITS;

public class Test4 {
	public static void main(String[] args) {
		try {
			SerialComManager scm = new SerialComManager();
			
			String PORT = null;
			int osType = SerialComManager.getOSType();
			if(osType == SerialComManager.OS_LINUX) {
				PORT = "/dev/ttyUSB0";
			}else if(osType == SerialComManager.OS_WINDOWS) {
				PORT = "COM51";
			}else if(osType == SerialComManager.OS_MAC_OS_X) {
				PORT = "/dev/cu.usbserial-A70362A3";
			}else if(osType == SerialComManager.OS_SOLARIS) {
				PORT = null;
			}else{
			}

			// open and configure port that will listen event
			long handle = scm.openComPort(PORT, true, true, true);
			scm.configureComPortData(handle, DATABITS.DB_8, STOPBITS.SB_1, PARITY.P_NONE, BAUDRATE.B9600, 0);
			scm.configureComPortControl(handle, FLOWCONTROL.HARDWARE, 'x', 'x', false, false);

			String[] config = scm.getCurrentConfiguration(handle);
			System.out.println(Arrays.toString(config));

			scm.closeComPort(handle);
		} catch (Exception e) {
			e.printStackTrace();
		}
	}
}