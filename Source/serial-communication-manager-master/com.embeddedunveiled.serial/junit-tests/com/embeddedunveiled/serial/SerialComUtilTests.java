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

package com.embeddedunveiled.serial;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

public class SerialComUtilTests {

	@Test(timeout=50)
	public void testDecodeBCD() {
		assertEquals("2.00", SerialComUtil.decodeBCD((short) 0x0200));
	}

	@Test(timeout=50)
	public void testByteArrayToHexString() {
		assertEquals("4F:4B", SerialComUtil.byteArrayToHexString("OK".getBytes(), ":"));
	}

}
