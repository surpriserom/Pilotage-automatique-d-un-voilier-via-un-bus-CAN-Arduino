/*
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

package test78;

import com.embeddedunveiled.serial.SerialComManager;
import com.embeddedunveiled.serial.vendor.SerialComMCHPSimpleIO;
import com.embeddedunveiled.serial.vendor.SerialComVendorLib;

public class Test78  {

	static SerialComManager scm;
	static int osType;
	static SerialComMCHPSimpleIO mchpsio = null;
	static String vendorSuppliedLib;
	static String libpath;

	public static void main(String[] args) {
		try {
			scm = new SerialComManager();
			osType = scm.getOSType();
			if(osType == SerialComManager.OS_LINUX) { 
			}else if(osType == SerialComManager.OS_WINDOWS) {
				libpath = "D:\\zz\\mchpsio";
				vendorSuppliedLib = "SimpleIO-UM.dll";
			}else if(osType == SerialComManager.OS_MAC_OS_X) {
			}else {
			}

			mchpsio = (SerialComMCHPSimpleIO) scm.getVendorLibInstance(SerialComVendorLib.VLIB_MCHP_SIMPLEIO, libpath, vendorSuppliedLib);

			try {
				System.out.println("initMCP2200() : " + mchpsio.initMCP2200(0x04d8, 0x00df));
			}catch (Exception e) {
				System.out.println("initMCP2200() : " + e.getMessage());
			}

			try {
				System.out.println("isConnected() : " + mchpsio.isConnected());
			}catch (Exception e) {
				System.out.println("isConnected() : " + e.getMessage());
			}

			try {
				System.out.println("configureMCP2200() : " + mchpsio.configureMCP2200((byte) 1, 9600, 1, 1, 
						false, true, true, false));
			}catch (Exception e) {
				System.out.println("configureMCP2200() : " + e.getMessage());
			}

			try {
				System.out.println("setPin() : " + mchpsio.setPin(1));
			}catch (Exception e) {
				System.out.println("setPin() : " + e.getMessage());
			}

			try {
				System.out.println("clearPin() : " + mchpsio.clearPin(1));
			}catch (Exception e) {
				System.out.println("clearPin() : " + e.getMessage());
			}

			try {
				System.out.println("readPinValue() : " + mchpsio.readPinValue(1));
			}catch (Exception e) {
				System.out.println("readPinValue() : " + e.getMessage());
			}

			try {
				System.out.println("readPin() : " + mchpsio.readPin(1));
			}catch (Exception e) {
				System.out.println("readPin() : " + e.getMessage());
			}

			try {
				System.out.println("writePort() : " + mchpsio.writePort(1));
			}catch (Exception e) {
				System.out.println("writePort() : " + e.getMessage());
			}

			try {
				System.out.println("readPort() : " + mchpsio.readPort());
			}catch (Exception e) {
				System.out.println("readPort() : " + e.getMessage());
			}

			try {
				System.out.println("readPortValue() : " + mchpsio.readPortValue());
			}catch (Exception e) {
				System.out.println("readPortValue() : " + e.getMessage());
			}

			try {
				System.out.println("selectDevice() : " + mchpsio.selectDevice(0));
			}catch (Exception e) {
				System.out.println("selectDevice() : " + e.getMessage());
			}

			try {
				System.out.println("getSelectedDevice() : " + mchpsio.getSelectedDevice());
			}catch (Exception e) {
				System.out.println("getSelectedDevice() : " + e.getMessage());
			}

			try {
				System.out.println("getNumOfDevices() : " + mchpsio.getNumOfDevices());
			}catch (Exception e) {
				System.out.println("getNumOfDevices() : " + e.getMessage());
			}

			try {
				System.out.println("getSelectedDeviceInfo() : " + mchpsio.getSelectedDeviceInfo());
			}catch (Exception e) {
				System.out.println("getSelectedDeviceInfo() : " + e.getMessage());
			}

			try {
				System.out.println("readEEPROM() : " + mchpsio.readEEPROM(0x00));
			}catch (Exception e) {
				System.out.println("readEEPROM() : " + e.getMessage());
			}

			//			try {
			//				System.out.println("writeEEPROM() : " + mchpsio.writeEEPROM(0x00, (short) 0x00));
			//			}catch (Exception e) {
			//				System.out.println("writeEEPROM() : " + e.getMessage());
			//			}

			try {
				System.out.println("fnRxLED() : " + mchpsio.fnRxLED(SerialComMCHPSimpleIO.ON));
			}catch (Exception e) {
				System.out.println("fnRxLED() : " + e.getMessage());
			}

			try {
				System.out.println("fnTxLED() : " + mchpsio.fnTxLED(SerialComMCHPSimpleIO.ON));
			}catch (Exception e) {
				System.out.println("fnTxLED() : " + e.getMessage());
			}

			try {
				System.out.println("hardwareFlowControl() : " + mchpsio.hardwareFlowControl(0));
			}catch (Exception e) {
				System.out.println("hardwareFlowControl() : " + e.getMessage());
			}

			try {
				System.out.println("fnULoad() : " + mchpsio.fnULoad(1));
			}catch (Exception e) {
				System.out.println("fnULoad() : " + e.getMessage());
			}

			try {
				System.out.println("fnSuspend() : " + mchpsio.fnSuspend(1));
			}catch (Exception e) {
				System.out.println("fnSuspend() : " + e.getMessage());
			}

			try {
				System.out.println("fnInvertUartPol() : " + mchpsio.fnInvertUartPol(1));
			}catch (Exception e) {
				System.out.println("fnInvertUartPol() : " + e.getMessage());
			}

			try {
				System.out.println("fnSetBaudRate() : " + mchpsio.fnSetBaudRate(115200));
			}catch (Exception e) {
				System.out.println("fnSetBaudRate() : " + e.getMessage());
			}

			try {
				System.out.println("configureIO() : " + mchpsio.configureIO((short) 1));
			}catch (Exception e) {
				System.out.println("configureIO() : " + e.getMessage());
			}

			try {
				System.out.println("configureIoDefaultOutput() : " + mchpsio.configureIoDefaultOutput((short)1, (short)1));
			}catch (Exception e) {
				System.out.println("configureIoDefaultOutput() : " + e.getMessage());
			}

		} catch (Exception e) {
			e.printStackTrace();
		}
	}
}
