package org.kde.kjas.server;

import java.awt.*;
import java.awt.event.*;
import java.io.*;

public class KJASConsole
    extends Frame
{
    private TextArea txt;

    public KJASConsole()
    {
        super("Konqueror Java Console");

        txt = new TextArea();
        txt.setEditable(false);

        Panel main = new Panel(new BorderLayout());
        Panel btns = new Panel(new BorderLayout());

        Button clear = new Button("Clear");
        Button close = new Button("Close");
        
        btns.add(clear, "West");
        btns.add(close, "East");

        main.add(txt, "Center");
        main.add(btns, "South");
        
        add( main );

        clear.addActionListener
        (
            new ActionListener() {
                public void actionPerformed(ActionEvent e) {
                    txt.setText("");
                }
            }
        );

        close.addActionListener
        (
            new ActionListener() {
                public void actionPerformed(ActionEvent e) {
                    setVisible(false);
                }
            }
        );

        addWindowListener
        (
            new WindowAdapter() {
                public void windowClosing(WindowEvent e) {
                    setVisible(false);
                }
            }
        );

        setSize(300, 300);

        PrintStream st = new PrintStream( new KJASConsoleStream(txt) );
        System.setOut(st);
        System.setErr(st);
        
        System.out.println( "Java VM version: " +
                            System.getProperty("java.version") );
        System.out.println( "Java VM vendor:  " +
                            System.getProperty("java.vendor") );
    }
}

class KJASConsoleStream
    extends OutputStream
{
    private TextArea txt;
    private FileOutputStream dbg_log;

    public KJASConsoleStream( TextArea _txt )
    {
        txt = _txt;

        try
        {
            if( Main.log )
            {
                dbg_log = new FileOutputStream( "/tmp/kjas.log" );
            }
        }
        catch( FileNotFoundException e ) {}
    }

    public void close() {}
    public void flush() {}
    public void write(byte[] b) {}
    public void write(int a) {}

    // Should be enough for the console
    public void write( byte[] bytes, int offset, int length )
    {
        try  // Just in case
        {
            String msg = new String( bytes, offset, length );
            synchronized( txt )
            {
                //get the caret position, and then get the new position
                int old_pos = txt.getCaretPosition();
                txt.append(msg);
                txt.setCaretPosition( old_pos + length );

                if( Main.log && dbg_log != null )
                {
                    dbg_log.write( msg.getBytes() );
                }
            }
        }
        catch(Throwable t) {}
    }
}

