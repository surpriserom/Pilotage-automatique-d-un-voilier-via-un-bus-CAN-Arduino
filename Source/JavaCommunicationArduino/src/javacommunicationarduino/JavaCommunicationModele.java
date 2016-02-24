package javacommunicationarduino;

import com.embeddedunveiled.serial.SerialComException;
import com.embeddedunveiled.serial.SerialComManager;
import com.embeddedunveiled.serial.SerialComManager.BAUDRATE;
import com.embeddedunveiled.serial.SerialComManager.DATABITS;
import com.embeddedunveiled.serial.SerialComManager.FLOWCONTROL;
import com.embeddedunveiled.serial.SerialComManager.PARITY;
import com.embeddedunveiled.serial.SerialComManager.STOPBITS;

/**
 *
 * @author romain
 */

public class JavaCommunicationModele 
{
    private String[] portList;
    private String currentPort;
    private boolean connected;
    private long connectionHandle;
    private SerialComManager scm;
    private BAUDRATE baudeRate;
    private InterfaceCommunication interfaceComm;
            
    public JavaCommunicationModele() 
    {
        try 
        {
            this.scm = new SerialComManager();
            this.portList = this.scm.listAvailableComPorts();
            this.connected = false;
            this.currentPort = "";
            this.baudeRate = BAUDRATE.B300;
                
	} catch (Exception e) 
        {
            e.printStackTrace();
	}
    }
    
    public void setInterfaceComm(InterfaceCommunication inter)
    {
        this.interfaceComm = inter;
    }
    
    public String[] getPortList()
    {
        return this.portList;
    }
    
    public boolean isConnected()
    {
        //on test la connection en vérifiant si l'on était connecter et si le port est encore présent
        if(this.connected)
        {
            boolean found = false;
            try
            {
                for(String port : this.scm.listAvailableComPorts())
                {
                    if(port.equalsIgnoreCase(this.currentPort))
                    {
                        found = true;
                    }
                }
                this.connected = found;
                if(!this.connected)
                {
                    this.closeConnection();
                }
            }
            catch(SerialComException e)
            {
                e.printStackTrace();
            }
        }
        return this.connected;
    }
    
    public String openConnection(int connection)
    {
        try
        {
            //on test si la valeur selectionner correspond a une valeur valide du tableau
            if(connection >= this.portList.length)
            {
                return "La valeur selectionne est trop grande";
            }
            this.currentPort = this.portList[connection];
            
            // try opening serial port for read and write without exclusive ownership
            this.connectionHandle = this.scm.openComPort(currentPort, true, true, true);
            
            // configure data communication related parameters
            this.scm.configureComPortData(this.connectionHandle, DATABITS.DB_8, STOPBITS.SB_1, PARITY.P_NONE, this.baudeRate, 0);
 
            // configure line control related parameters
            this.scm.configureComPortControl(this.connectionHandle , FLOWCONTROL.NONE, 'x', 'x', false, false);
            this.connected = true;
            
            this.scm.registerDataListener(this.connectionHandle, this.interfaceComm.dataListener);
        }
        catch(SerialComException e)
        {
            this.connected = false;
            return e.toString();
        }
        return "Connection reussit au port "+this.currentPort;
    }
    
    public String closeConnection()
    {
        this.connected = false; // si erreur, la connection etait déja fermé
        try
        {
            this.scm.unregisterDataListener(this.interfaceComm.dataListener);
            this.scm.closeComPort(this.connectionHandle);
            this.currentPort = "";
        }
        catch(SerialComException e)
        {
            return e.toString();
        }
        return "Connection terminer";
    }
    
    public String refreshPortList()
    {
        try
        {
            this.portList = this.scm.listAvailableComPorts();
        }
        catch(SerialComException e)
        {
            return e.toString();
        }
        return "Liste de port Serie rafraichie";
    }
    
    public String sendData(String data)
    {
        try
        {
            this.scm.writeString(this.connectionHandle, data, 0);
        }
        catch(SerialComException e)
        {
            return e.toString();
        }
        return "Commande Envoyer";
    }
    
    public String setBaudRate(int value)
    {
        String txt;
        switch(value)
        {
            case 300 :
                this.baudeRate = BAUDRATE.B300;
                txt = "BaudeRate set to 300";
                break;
            case 600 :
                this.baudeRate = BAUDRATE.B600;
                txt = "BaudeRate set to 600";
                break;
            case 1200 :
                this.baudeRate = BAUDRATE.B1200;
                txt = "BaudeRate set to 1200";
                break;
            case 2400 :
                this.baudeRate = BAUDRATE.B2400;
                txt = "BaudeRate set to 2400";
                break;
            case 4800 :
                this.baudeRate = BAUDRATE.B4800;
                txt = "BaudeRate set to 2400";
                break;
            case 9600 :
                this.baudeRate = BAUDRATE.B9600;
                txt = "BaudeRate set to 9600";
                break;
            case 14400 :
                this.baudeRate = BAUDRATE.B14400;
                txt = "BaudeRate set to 14400";
                break;
            case 19200 :
                this.baudeRate = BAUDRATE.B19200;
                txt = "BaudeRate set to 19200";
                break;
            case 28800 :
                this.baudeRate = BAUDRATE.B28800;
                txt = "BaudeRate set to 28800";
                break;
            case 38400 :
                this.baudeRate = BAUDRATE.B38400;
                txt = "BaudeRate set to 38400";
                break;
            case 57600 :
                this.baudeRate = BAUDRATE.B57600;
                txt = "BaudeRate set to 57600";
                break;
            case 115200 :
                this.baudeRate = BAUDRATE.B115200;
                txt = "BaudeRate set to 115200";
                break;
            default :
                this.baudeRate = BAUDRATE.B300;
                txt = "BaudeRate set to 300";
                break;
        }
        if(isConnected())
        {
            try
            {
                // configure data communication related parameters
                this.scm.configureComPortData(this.connectionHandle, DATABITS.DB_8, STOPBITS.SB_1, PARITY.P_NONE, this.baudeRate, 0);
                txt = txt+"\n Vitesse de la connection ouverte modifier";
            }
            catch(SerialComException e)
            {
                return e.toString();
            }
        }
        return txt;
    }
    
    
}
