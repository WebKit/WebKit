    package org.kde.kjas.server;

import java.io.*;
import java.security.*;
import java.net.*;

/**
 *  KJAS server recognizes these variablers:
 *    kjas.debug - makes server actions verbose
 *    kjas.showConsole - shows Java Console window
 *    kjas.log - save a transcript of the debug output to /tmp/kjas.log
 */

public class Main
{
    //We need to save a reference to the original stdout
    //for sending messages back
    public  static final PrintStream         protocol_stdout;
    public  static final KJASProtocolHandler protocol;
    public  static final KJASConsole         console;
    private static final boolean             show_console;
    public  static final boolean             Debug;
    public  static final boolean             log;
    private static boolean                   good_jdk = true;

    /**************************************************************************
     * Initialization
     **************************************************************************/
    static
    {
        if( System.getProperty( "kjas.debug" ) != null )
            Debug = true;
        else
            Debug = false;

        if( System.getProperty( "kjas.showConsole" ) != null )
            show_console = true;
        else
            show_console = false;

        if( System.getProperty( "kjas.log" ) != null )
            log = true;
        else
            log = false;

        protocol_stdout = System.out;
        console         = new KJASConsole();
        protocol        = new KJASProtocolHandler( System.in, protocol_stdout );

        Main.debug( "JVM version = " + System.getProperty( "java.version" ) );
        String version = System.getProperty("java.version").substring( 0, 3 );
        Main.debug( "JVM numerical version = " + version );
        try
        {
            float java_version = Float.parseFloat( version );
            if( java_version < 1.2 )
                good_jdk = false;
        } catch( NumberFormatException e )
        {
            good_jdk = false;
        }
    }

    /**************************************************************************
     * Public Utility functions available to the KJAS framework
     **************************************************************************/
    public static void debug( String msg )
    {
        if( Debug )
        {
            System.out.println( "KJAS: " + msg );
        }
    }

    public static void kjas_err( String msg, Exception e )
    {
        System.err.println( msg );
        System.err.println( "Backtrace: " );
        e.printStackTrace();
    }

    public static void kjas_err( String msg, Throwable t )
    {
        System.err.println( msg );
        t.printStackTrace();
    }


    /**************************************************************************
     * Main- create the command loop
     **************************************************************************/
    public static void main( String[] args )
    {
        if( !good_jdk )
        {
            console.setVisible( true );
            System.err.println( "ERROR: This version of Java is not supported for security reasons." );
            System.err.println( "\t\tPlease use Java version 1.2 or higher." );
            return;
        }

        if( show_console )
            console.setVisible( true );


        try  //Check for the JSSE packages, and install them
        {
            //set this property first
            System.setProperty("java.protocol.handler.pkgs", "com.sun.net.ssl.internal.www.protocol");
            if( Security.getProvider( "SunJSSE" ) == null )
            {
                Class provider = Class.forName("com.sun.net.ssl.internal.ssl.Provider");
                if( provider != null )
                {
                    Main.debug( "adding Security Provider" );
                    Provider p = (Provider) provider.newInstance();
                    Security.addProvider( p );
                }
                else
                    Main.debug( "could not get class: com.sun.net.ssl.internal.ssl.Provider" );
            }
            else
                Main.debug( "could not get provider: SunJSSE" );
        } catch( Exception e )
        {
            System.out.println( "Unable to load JSSE SSL stream handler, https support not available" );
        }

        //start the command parsing
        protocol.commandLoop();
    }

}
