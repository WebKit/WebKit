import java.awt.*;
import java.awt.event.*;
import java.applet.*;
import javax.swing.*;
import java.io.*;
import java.net.*;
import java.awt.datatransfer.*;

public class BadApplet extends JApplet {
    JTabbedPane tabs        = new JTabbedPane();
    JPanel FileSystemTests  = new JPanel();
    JPanel NetworkTests     = new JPanel();
    JPanel EnvironmentTests = new JPanel();

    JButton writeFileButton      = new JButton("Write File");
    JButton readFileButton       = new JButton("Read File");
    JButton connectSocketButton  = new JButton("Connect Socket");
    JButton frameButton          = new JButton("Open Frame Without Warning Tag");
    JButton readSystemPropButton = new JButton("Read System Property");
    JButton printButton          = new JButton("Print");
    JButton clipBoardButton      = new JButton("Read Clipboard");

    JTextField writePath         = new JTextField( "/amd/ns/root/home/sbarnes/test.txt" );
    JTextField readPath          = new JTextField("/amd/ns/root/home/sbarnes/test.txt");
    JTextField url               = new JTextField("URL");
    JTextField port              = new JTextField("port");
    JTextField systemProp        = new JTextField("os.name");
    JTextField output            = new JTextField();

    //Construct the applet
    public BadApplet() {
        try {
            //event handlers ******************************************************
            writeFileButton.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(ActionEvent e) {
                    writeFileButton_actionPerformed(e);
                }
            });
            readFileButton.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(ActionEvent e) {
                    readFileButton_actionPerformed(e);
                }
            });
            connectSocketButton.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(ActionEvent e) {
                    connectSocketButton_actionPerformed(e);
                }
            });
            frameButton.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(ActionEvent e) {
                frameButton_actionPerformed(e);
                }
            });
            readSystemPropButton.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(ActionEvent e) {
                    readSystemPropButton_actionPerformed(e);
                }
            });
            printButton.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(ActionEvent e) {
                    printButton_actionPerformed(e);
                }
            });
            clipBoardButton.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(ActionEvent e) {
                    clipBoard_actionPerformed(e);
                }
            });

            //do layout ***********************************************************
            getContentPane().setLayout( new BorderLayout() );

            FileSystemTests.setLayout( new FlowLayout( FlowLayout.LEFT ) );
            FileSystemTests.add( writeFileButton );
            FileSystemTests.add( writePath );
            FileSystemTests.add( readFileButton );
            FileSystemTests.add( readPath );

            NetworkTests.setLayout( new FlowLayout( FlowLayout.LEFT ) );
            NetworkTests.add( connectSocketButton );
            NetworkTests.add( url );
            NetworkTests.add( port );

            EnvironmentTests.setLayout( new FlowLayout( FlowLayout.LEFT ) );
            EnvironmentTests.add( frameButton );
            EnvironmentTests.add( readSystemPropButton );
            EnvironmentTests.add( systemProp );
            EnvironmentTests.add( printButton );
            EnvironmentTests.add( clipBoardButton );

            tabs.add( FileSystemTests, "File System" );
            tabs.add( NetworkTests, "Network" );
            tabs.add( EnvironmentTests, "Environment" );

            this.getContentPane().add( tabs, BorderLayout.CENTER );
            this.getContentPane().add( output, BorderLayout.SOUTH );
        }
        catch(Exception e) {
            e.printStackTrace();
        }
    }

    public void paint( Graphics g )
    {
        System.out.println( "graphics g = " + g );
        System.out.println( "clip area = " + g.getClip() );
        System.out.println( "bounds of the clip area = " + g.getClipBounds() );

        super.paint( g );
    }

    //Initialize the applet
    public void init() {}

    void writeFileButton_actionPerformed(ActionEvent e) {
        try{
            PrintWriter writer = new PrintWriter(new FileOutputStream(writePath.getText()));
            writer.println("Here is some text");
            writer.close();
            output.setText("Write was successfull");
        } catch (Exception ex){output.setText(ex.getMessage());}
    }

    void readSystemPropButton_actionPerformed(ActionEvent e) {
        try{
            output.setText(System.getProperty(systemProp.getText()));
        } catch (Exception ex){output.setText("Error getting prop: " + ex.getMessage());}
    }

    void readFileButton_actionPerformed(ActionEvent e) {
        try{
            BufferedReader reader = new BufferedReader(new FileReader(readPath.getText()));
            output.setText("Read was successfull: " + reader.readLine());
        } catch (Exception ex){output.setText(ex.getMessage());}
    }

    void connectSocketButton_actionPerformed(ActionEvent e) {
        try{
            Integer thePort = new Integer(port.getText());
            Socket socket = new Socket(url.getText(), thePort.intValue());
            socket.getOutputStream();
            output.setText("Socket connection successfull");
        } catch (Exception ex){output.setText("Socket unsuccessfull: " + ex.getMessage());}
    }

    void frameButton_actionPerformed(ActionEvent e) {
        JFrame frame = new JFrame("Does this Frame have a warning sign");
        frame.setSize(200,200);
        frame.show();
        if (frame.getWarningString() == null)
            output.setText("No warning string in frame");
        else
            output.setText(frame.getWarningString());
    }

    void clipBoard_actionPerformed(ActionEvent e) {
        try {
            Clipboard clip = Toolkit.getDefaultToolkit().getSystemClipboard();

            Transferable trans = clip.getContents(null);
            if (trans == null){
                output.setText("Clipboard is empty");
                return;
            }
            output.setText((String)trans.getTransferData(DataFlavor.stringFlavor));
        }catch(Exception ex){ex.getMessage();}
    }

    void printButton_actionPerformed(ActionEvent e) {
        try{
            JFrame testFrame = new JFrame("test");
            testFrame.getContentPane().add(this, BorderLayout.CENTER);
            PrintJob printer = Toolkit.getDefaultToolkit().getPrintJob(testFrame, "Applet Print Test", null);

            if (printer == null){
                output.setText("PrintJob is null");
                return;
            }

            Graphics g = printer.getGraphics();
            g.drawString("This is the applet print test", 50, 50);
            g.dispose();
            printer.end();
        }catch(Exception ex){ex.getMessage();}
    }

    //Main method
    public static void main(String[] args) {
        BadApplet applet = new BadApplet();

        JFrame frame = new JFrame();
        frame.setDefaultCloseOperation( WindowConstants.DISPOSE_ON_CLOSE );
        frame.setTitle("Applet Frame");
        frame.getContentPane().add(applet, BorderLayout.CENTER);
        frame.setSize(400,320);
        frame.setVisible(true);

        applet.init();
        applet.start();
    }

}
