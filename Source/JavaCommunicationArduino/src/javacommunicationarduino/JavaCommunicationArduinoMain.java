package javacommunicationarduino;
        
/**
 * launching class
 * @author romain
 */
public class JavaCommunicationArduinoMain {    
    
    /**
     * @param args the command line arguments - not used
     */
    public static void main(String[] args) 
    {
        JavaCommunicationModele model = new JavaCommunicationModele();
        InterfaceCommunication disp = new InterfaceCommunication(model);
        model.setInterfaceComm(disp);
        disp.setVisible(true);
    }
}
