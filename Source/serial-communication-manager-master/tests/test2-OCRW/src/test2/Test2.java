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

package test2;

import com.embeddedunveiled.serial.SerialComManager;
import com.embeddedunveiled.serial.SerialComManager.BAUDRATE;
import com.embeddedunveiled.serial.SerialComManager.DATABITS;
import com.embeddedunveiled.serial.SerialComManager.FLOWCONTROL;
import com.embeddedunveiled.serial.SerialComManager.PARITY;
import com.embeddedunveiled.serial.SerialComManager.STOPBITS;

public final class Test2 {
	public static void main(String[] args) {
		try {
			// get serial communication manager instance
			SerialComManager scm = new SerialComManager();

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

			// try opening serial port for read and write without exclusive ownership
			long handle = scm.openComPort(PORT, true, true, true);
			// configure data communication related parameters
			scm.configureComPortData(handle, DATABITS.DB_8, STOPBITS.SB_1, PARITY.P_NONE, BAUDRATE.B115200, 0);
			// configure line control related parameters
			scm.configureComPortControl(handle, FLOWCONTROL.NONE, 'x', 'x', false, false);
			
			long handle1 = scm.openComPort(PORT1, true, true, true);
			scm.configureComPortData(handle1, DATABITS.DB_8, STOPBITS.SB_1, PARITY.P_NONE, BAUDRATE.B115200, 0);
			scm.configureComPortControl(handle1, FLOWCONTROL.NONE, 'x', 'x', false, false);
			
			scm.writeSingleByte(handle, (byte) 'A');
			Thread.sleep(1000);
			byte[] datarcv = scm.readSingleByte(handle1);
			System.out.println("readSingleByte is : " + datarcv[0]);
			
			String data111 = scm.readString(handle1);
			System.out.println("data read for 1 byte is : " + data111);
			
			// test single byte
			if(scm.writeString(handle, "1", 0) == true) {
				System.out.println("write success 1 byte");
			}
			Thread.sleep(1000);
			String data = scm.readString(handle1);
			System.out.println("data read for 1 byte is : " + data);
			
			// test 2 byte
			if(scm.writeString(handle, "22", 0) == true) {
				System.out.println("write success 2 byte");
			}
			Thread.sleep(1000);
			data = scm.readString(handle1);
			System.out.println("data read for 2 byte is : " + data);
			
			// test 3 byte
			if(scm.writeString(handle, "333", 0) == true) {
				System.out.println("write success 3 byte");
			}
			Thread.sleep(1000);
			data = scm.readString(handle1);
			System.out.println("data read for 3 byte is : " + data);
			
			// test 4 byte
			if(scm.writeString(handle, "4444", 0) == true) {
				System.out.println("write success 4 byte");
			}
			Thread.sleep(1000);
			data = scm.readString(handle1);
			System.out.println("data read for 4 byte is : " + data);
			
			// test 5 byte
			if(scm.writeString(handle, "55555", 0) == true) {
				System.out.println("write success 5 byte");
			}
			Thread.sleep(1000);
			data = scm.readString(handle1);
			System.out.println("data read for 5 byte is : " + data);
			
			// test 10 byte
			if(scm.writeString(handle, "1000000000", 0) == true) {
				System.out.println("write success 10 byte");
			}
			Thread.sleep(1000);
			data = scm.readString(handle1);
			System.out.println("data read for 10 byte is : " + data);

			scm.closeComPort(handle);
			scm.closeComPort(handle1);
		} catch (Exception e) {
			e.printStackTrace();
		}
	}
}