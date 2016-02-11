/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package javacommunicationarduino;

import java.util.Enumeration;
import javax.comm.CommPortIdentifier;
        
/**
 *
 * @author romain
 */
public class JavaCommunicationArduino {    
    
    /**
     * @param args the command line arguments
     */
    public static void main(String[] args) 
    {
        int i = 0;
        Enumeration<CommPortIdentifier> portList;
        
        portList = CommPortIdentifier.getPortIdentifiers();
        System.out.println("has more: " + portList.hasMoreElements());
        while(portList.hasMoreElements()){
            System.out.print("Port"+i+": ");
             CommPortIdentifier portId = (CommPortIdentifier) portList.nextElement();
               if (portId.getPortType() == CommPortIdentifier.PORT_PARALLEL)
               {
                    System.out.println(portId.getName() + "<= is parallele");
               }
               else
               {
                     System.out.println(portId.getName());
               }

        }
    }
    
}
