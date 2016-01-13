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

import java.util.HashMap;

import nz.co.fortytwo.freeboard.server.util.Constants;
import nz.co.fortytwo.freeboard.server.util.Util;

import org.apache.camel.Produce;
import org.apache.camel.ProducerTemplate;
import org.apache.commons.lang3.StringUtils;
import org.apache.commons.lang3.math.NumberUtils;
import org.apache.log4j.Logger;


/**
 * Holder for some useful methods for processors
 * @author robert
 *
 */
public class FreeboardProcessor {
	
	private static Logger logger = Logger.getLogger(FreeboardProcessor.class);
	
	@Produce(uri = "seda:nmeaOutput")
    ProducerTemplate producer;
	/**
	 * If a processor generates an NMEA string, then this method is a convenient way to send it to the NMEA stream
	 * 
	 * @param nmea
	 */
	public void sendNmea(String nmea){
		producer.sendBody(nmea);
	}
	/**
	 * Appends key/value pair as KEY:value, 
	 * @param builder
	 * @param key
	 * @param value
	 */
	public void appendValue(StringBuilder builder, String key, Object value){
		builder.append(key);
		builder.append(":");
		builder.append(value);
		builder.append(",");
	}

	/**
	 * Round to specified decimals
	 * @param val
	 * @param places
	 * @return
	 */
	public double round(double val, int places){
		return Util.round(val, places);
	}
	
	/**
	 * Converts a msg string into a hashmap
	 * @param msg
	 * @return
	 */
	public HashMap<String, Object> stringToHashMap(String msg){
		if(logger.isDebugEnabled())logger.debug("Mesg to map:"+msg);
		HashMap<String, Object> map = new HashMap<String, Object>();
		//AIS
		//!AIVDM,1,1,,B,15MwkRUOidG?GElEa<iQk1JV06Jd,0*6D
		if(msg.startsWith("$")){
			if(logger.isDebugEnabled())logger.debug("Mesg to NMEA in map:"+msg);
			map.put(Constants.NMEA, msg);
		}else if(msg.startsWith("!AIVDM")){
			if(logger.isDebugEnabled())logger.debug("Mesg to NMEA in map:"+msg);
			map.put(Constants.AIS, msg);
		}else{
			if(logger.isDebugEnabled())logger.debug("Mesg split in map:"+msg);
			String[] bodyArray = msg.split(",");
			//reuse
			String[] pair;
			for(String s:bodyArray){
				if(StringUtils.isNotBlank(s)){
					pair=s.split(":");
					if(pair==null || pair.length!=2)continue;
					
					if(NumberUtils.isNumber(pair[1])){
						Object val;
						if(pair[1].indexOf(".")>0){
							val=Double.valueOf(pair[1]);
						}else{
							val=Integer.valueOf(pair[1]);
						}
						map.put(pair[0], val);
					}else{
						map.put(pair[0], pair[1]);
					}
				}
			}
		}
		return map;
	}
	
	/**
	 * Convert hashmap of key/value pairs back to String
	 * @param map
	 * @return
	 */
	public String hashMapToString(HashMap<String, Object> map){
		StringBuilder builder = new StringBuilder();
		for(String s:map.keySet()){
			if(s.equals(Constants.NMEA)){
				builder.append(map.get(s));
			}else{
				appendValue(builder, s, map.get(s).toString());
			}
		}
		return builder.toString();
	}
	
	/**
	 * Convert hashmap of key/value pairs back to JSON
	 * @param map
	 * @return
	 */
	public String hashMapToJSON(HashMap<String, Object> map){
		
		return null;//builder.toString();
	}
	public HashMap<String, Object> handle(HashMap<String, Object> map) {
		// TODO Auto-generated method stub
		return map;
	}
}
