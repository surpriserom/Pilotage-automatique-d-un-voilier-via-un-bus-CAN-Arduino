package javacommunicationarduino;
        
/**
 *
 * @author romain
 */
public class JavaCommunicationArduinoMain {    
    
    /**
     * @param args the command line arguments
     */
    public static void main(String[] args) 
    {
        JavaCommunicationModele model = new JavaCommunicationModele();
        InterfaceCommunication disp = new InterfaceCommunication(model);
        model.setInterfaceComm(disp);
        disp.setVisible(true);
    }
}
