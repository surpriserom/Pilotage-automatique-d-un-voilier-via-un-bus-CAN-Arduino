/*
 * Copyright 2012,2013 Robert Huitema robert@42.co.nz
 * 
 * This file is part of FreeBoard. (http://www.42.co.nz/freeboard)
 *
 *  FreeBoard is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.

 *  FreeBoard is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with FreeBoard.  If not, see <http://www.gnu.org/licenses/>.
 */

package nz.co.fortytwo.freeboard.server;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.HashMap;

import junit.framework.Assert;
import nz.co.fortytwo.freeboard.server.util.Constants;
import nz.co.fortytwo.freeboard.server.util.Util;

import org.apache.log4j.Logger;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

public class NMEAProcessorTest {

	private static Logger logger = Logger.getLogger(NMEAProcessorTest.class);
	
	@Before
	public void setUp() throws Exception {
	}

	@After
	public void tearDown() throws Exception {
	}

	@Test
	public void shouldHandleGPRMC(){
		 String nmea1 = "$GPRMC,144629.20,A,5156.91111,N,00434.80385,E,0.295,,011113,,,A*78";
		 String nmea2 = "$GPRMC,144629.30,A,5156.91115,N,00434.80383,E,1.689,,011113,,,A*73";
		 String nmea3 = "$GPRMC,144629.50,A,5156.91127,N,00434.80383,E,1.226,,011113,,,A*75";
		 NMEAProcessor processor = new NMEAProcessor();
		 HashMap<String, Object> map =new HashMap<>();
		 map.put(Constants.NMEA, nmea1);
		 map = processor.handle(map);
		 logger.debug(map);
		 Assert.assertTrue(map.containsKey(Constants.LAT));
		 logger.debug("Lat :"+map.get(Constants.LAT));
	}
	@Test
	public void shouldHandleCruzproXDR() throws FileNotFoundException, IOException {
		NMEAProcessor processor = new NMEAProcessor();
		HashMap<String, Object> map = new HashMap<String, Object>();
		Util.getConfig(null).setProperty("freeboard.nmea.YXXDR.MaxVu110", "RPM,EVV,SKIP,EPP,ETT");
		map.put(Constants.NMEA, "$YXXDR,G,0004,,G,12.27,,G,,,G,003.3,,G,0012,,MaxVu110*4E");
		processor.handle(map);
		//RPM,EVV,DBT,EPP,ETT
		Assert.assertEquals(4.0,map.get(Constants.ENGINE_RPM));
		Assert.assertEquals(12.27,map.get(Constants.ENGINE_VOLTS));
		Assert.assertEquals(null,map.get(Constants.DEPTH_BELOW_TRANSDUCER));
		Assert.assertEquals(3.3,map.get(Constants.ENGINE_OIL_PRESSURE));
		Assert.assertEquals(12.0,map.get(Constants.ENGINE_TEMP));
	}
	
	@Test
	public void shouldHandleSkipValue() throws FileNotFoundException, IOException {
		NMEAProcessor processor = new NMEAProcessor();
		HashMap<String, Object> map = new HashMap<String, Object>();
		//freeboard.nmea.YXXDR.MaxVu110=RPM,EVV,DBT,EPP,ETT
		Util.getConfig(null).setProperty("freeboard.nmea.YXXDR.MaxVu110", "RPM,EVV,SKIP,EPP,ETT");
		map.put(Constants.NMEA, "$YXXDR,G,0004,,G,12.27,,G,,,G,003.3,,G,0012,,MaxVu110*4E");
		processor.handle(map);
		//RPM,EVV,DBT,EPP,ETT
		Assert.assertEquals(4.0,map.get(Constants.ENGINE_RPM));
		Assert.assertEquals(12.27,map.get(Constants.ENGINE_VOLTS));
		Assert.assertTrue(!map.containsKey(Constants.DEPTH_BELOW_TRANSDUCER));
		Assert.assertEquals(3.3,map.get(Constants.ENGINE_OIL_PRESSURE));
		Assert.assertEquals(12.0,map.get(Constants.ENGINE_TEMP));
	}
	@Test
	public void shouldRejectMismatchedValues() throws FileNotFoundException, IOException {
		NMEAProcessor processor = new NMEAProcessor();
		HashMap<String, Object> map = new HashMap<String, Object>();
		//freeboard.nmea.YXXDR.MaxVu110=RPM,EVV,DBT,EPP,ETT
		Util.getConfig(null).setProperty("freeboard.nmea.YXXDR.MaxVu110", "RPM,EVV,SKIP,EPP");
		map.put(Constants.NMEA, "$YXXDR,G,0004,,G,12.27,,G,,,G,003.3,,G,0012,,MaxVu110*4E");
		processor.handle(map);
		//RPM,EVV,DBT,EPP,ETT
		Assert.assertTrue(!map.containsKey(Constants.ENGINE_RPM));
		Assert.assertTrue(!map.containsKey(Constants.ENGINE_VOLTS));
		Assert.assertTrue(!map.containsKey(Constants.DEPTH_BELOW_TRANSDUCER));
		Assert.assertTrue(!map.containsKey(Constants.ENGINE_OIL_PRESSURE));
		Assert.assertTrue(!map.containsKey(Constants.ENGINE_TEMP));
	}

}
