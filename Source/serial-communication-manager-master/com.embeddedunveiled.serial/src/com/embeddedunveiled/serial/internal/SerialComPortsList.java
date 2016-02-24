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

package com.embeddedunveiled.serial.internal;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.TreeSet;
import java.util.regex.Pattern;
import com.embeddedunveiled.serial.SerialComManager;

/**
 * <p>Finds all serial ports known to system and return them in sorted alphanumeric order.</p>
 * 
 * @author Rishi Gupta
 */
public final class SerialComPortsList {

	private int osType = -1;
	private SerialComPortJNIBridge mComPortJNIBridge = null;
	private static final Pattern Sol_regExpPattern = Pattern.compile("[0-9]*|[a-z]*");
	private static final String Sol_search_path = "/dev/term/";

	private static final Comparator<String> comparator = new Comparator<String>() {

		@Override
		public int compare(String valueA, String valueB) {

			if(valueA.equalsIgnoreCase(valueB)){
				return valueA.compareTo(valueB);
			}

			int minLength = Math.min(valueA.length(), valueB.length());
			int shiftA = 0;
			int shiftB = 0;

			for(int i = 0; i < minLength; i++){
				char charA = valueA.charAt(i - shiftA);
				char charB = valueB.charAt(i - shiftB);
				if(charA != charB){
					if(Character.isDigit(charA) && Character.isDigit(charB)){
						int[] resultsA = getNumberAndLastIndex(valueA, i - shiftA);
						int[] resultsB = getNumberAndLastIndex(valueB, i - shiftB);

						if(resultsA[0] != resultsB[0]){
							return resultsA[0] - resultsB[0];
						}
						if(valueA.length() < valueB.length()){
							i = resultsA[1];
							shiftB = resultsA[1] - resultsB[1];
						}else {
							i = resultsB[1];
							shiftA = resultsB[1] - resultsA[1];
						}
					}else {
						if(Character.toLowerCase(charA) - Character.toLowerCase(charB) != 0){
							return Character.toLowerCase(charA) - Character.toLowerCase(charB);
						}
					}
				}
			}
			return valueA.compareToIgnoreCase(valueB);
		}

		private int[] getNumberAndLastIndex(String str, int startIndex) {
			String numVal = "";
			int[] val = {-1, startIndex};

			for(int i = startIndex; i < str.length(); i++){
				val[1] = i;
				char c = str.charAt(i);
				if(Character.isDigit(c)){
					numVal += c;
				}else {
					break;
				}
			}

			try {
				val[0] = Integer.valueOf(numVal);
			} catch (Exception e) {
				//Do nothing
			}
			return val;
		}
	};

	/** Allocates a new SerialComPortsList object. */
	public SerialComPortsList(SerialComPortJNIBridge mComPortJNIBridge, int osType) {
		this.mComPortJNIBridge = mComPortJNIBridge;
		this.osType = osType;
	}

	/**
	 * <p>This methods find all ports known to system at this instant.</p>
	 * <p>For Linux, Windows and Mac OS, ports are found with the help of OS specific facilities/API.</p>
	 * <p>For Solaris, a predefined port naming pattern is matched.</p>
	 * 
	 * @return array of ports found on system or null
	 */
	public String[] listAvailableComPorts() {
		if(osType != SerialComManager.OS_SOLARIS) {  // For Linux, Mac, Windows get list from native library.
			ArrayList<String> portsIdentified = new ArrayList<String>();
			String[] ports = mComPortJNIBridge.listAvailableComPorts();
			if(ports != null) {
				for(String portName : ports){
					portsIdentified.add(portName);
				}
				Collections.sort(portsIdentified, comparator);
				return portsIdentified.toArray(new String[portsIdentified.size()]);
			}
			return null;
		}else {                                           // For Solaris match the pre-known pattern for names.
			String[] portsIdentified = new String[]{};
			File dir = new File(Sol_search_path);
			if(dir.exists() && dir.isDirectory()) {
				File[] nodes = dir.listFiles();
				if(nodes.length > 0) {
					TreeSet<String> portsTree = new TreeSet<String>(comparator);
					for(File file : nodes) {
						String fileName = file.getName();
						if(!file.isDirectory() && !file.isFile() && Sol_regExpPattern.matcher(fileName).find()) {
							portsTree.add(Sol_search_path + fileName);
						}
					}
					portsIdentified = portsTree.toArray(portsIdentified); // return our findings.
				} else {
					return null; // no ports exist
				}
			} else {
				return null; // The look up path directory either does not exist or is not directory.
			}
			return portsIdentified;
		}
	}
}
