package org.kde.kjas.server;

import java.applet.*;
import java.util.*;
import java.net.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;

/**
 * The context in which applets live.
 */
public class KJASAppletContext implements AppletContext
{
    private Hashtable stubs;
    private Hashtable images;

    private String myID;
    private KJASAppletClassLoader loader;
    private boolean active;
    /**
     * Create a KJASAppletContext
     */
    public KJASAppletContext( String _contextID )
    {
        stubs  = new Hashtable();
        images = new Hashtable();
        myID   = _contextID;
        active = true;
    }

    public String getID()
    {
        return myID;
    }

    public void createApplet( String appletID, String name,
                              String className, String docBase,
                              String codeBase, String archives,
                              String width, String height,
                              String windowName, Hashtable params )
    {
        //do kludges to support mess with parameter table and
        //the applet variables
        String key = new String( "archive" ).toUpperCase();
        if( archives == null )
        {
            if( params.containsKey( key ) )
                archives = (String)params.get( key );
        }
        else
        {
            if( !params.containsKey( key ) )
                params.put( key, archives );
        }

        key = new String( "codebase" ).toUpperCase();
        if( codeBase == null )
        {
            if( params.containsKey( key ) )
                codeBase = (String) params.get( key );
        }

        key = new String( "width" ).toUpperCase();
        if( !params.containsKey( key ) )
            params.put( key, width );
        key = new String( "height" ).toUpperCase();
        if( !params.containsKey( key ) )
            params.put( key, height );

        try
        {
            KJASAppletClassLoader loader =
                KJASAppletClassLoader.getLoader( docBase, codeBase );
            if( archives != null )
            {
                StringTokenizer parser = new StringTokenizer( archives, ",", false );
                while( parser.hasMoreTokens() )
                {
                    String jar = parser.nextToken().trim();
                    loader.addArchiveName( jar );
                }
            }
            loader.paramsDone();

            KJASAppletStub stub = new KJASAppletStub
            (
                this, appletID, loader.getCodeBase(),
                loader.getDocBase(), name, className,
                new Dimension( Integer.parseInt(width), Integer.parseInt(height) ),
                params, windowName, loader
            );
            stubs.put( appletID, stub );

            stub.createApplet();
        }
        catch ( Exception e )
        {
            Main.kjas_err( "Something bad happened in createApplet: " + e, e );
        }
    }

    public void initApplet( String appletID )
    {
        KJASAppletStub stub = (KJASAppletStub) stubs.get( appletID );
        if( stub == null )
        {
            Main.debug( "could not init and show applet: " + appletID );
        }
        else
        {
            stub.initApplet();
        }
    }

    public void destroyApplet( String appletID )
    {
        KJASAppletStub stub = (KJASAppletStub) stubs.get( appletID );

        if( stub == null )
        {
            Main.debug( "could not destroy applet: " + appletID );
        }
        else
        {
            Main.debug( "stopping applet: " + appletID );
            stub.die();

            stubs.remove( appletID );
        }
    }

    public void startApplet( String appletID )
    {
        KJASAppletStub stub = (KJASAppletStub) stubs.get( appletID );
        if( stub == null )
        {
            Main.debug( "could not start applet: " + appletID );
        }
        else
        {
            stub.startApplet();
        }
    }

    public void stopApplet( String appletID )
    {
        KJASAppletStub stub = (KJASAppletStub) stubs.get( appletID );
        if( stub == null )
        {
            Main.debug( "could not stop applet: " + appletID );
        }
        else
        {
            stub.stopApplet();
        }
    }

    public void destroy()
    {
        Enumeration e = stubs.elements();
        while ( e.hasMoreElements() )
        {
            KJASAppletStub stub = (KJASAppletStub) e.nextElement();
            stub.die();
        }

        stubs.clear();
        active = false;
    }

    /***************************************************************************
    **** AppletContext interface
    ***************************************************************************/
    public Applet getApplet( String appletName )
    {
        if( active )
        {
            Enumeration e = stubs.elements();
            while( e.hasMoreElements() )
            {
                KJASAppletStub stub = (KJASAppletStub) e.nextElement();

                if( stub.getAppletName().equals( appletName ) )
                    return stub.getApplet();
            }
        }

        return null;
    }

    public Enumeration getApplets()
    {
        if( active )
        {
            Vector v = new Vector();
            Enumeration e = stubs.elements();
            while( e.hasMoreElements() )
            {
                KJASAppletStub stub = (KJASAppletStub) e.nextElement();
                v.add( stub );
            }

            return v.elements();
        }

        return null;
    }

    public AudioClip getAudioClip( URL url )
    {
        Main.debug( "getAudioClip, url = " + url );

        return new KJASSoundPlayer( url );
    }

    public void addImage( String url, byte[] data )
    {
        Main.debug( "addImage for url = " + url );
        images.put( url, data );
    }
    
    public Image getImage( URL url )
    {
        if( active && url != null )
        {
            //check with the Web Server        
            String str_url = url.toString();
            Main.debug( "getImage, url = " + str_url );
            Main.protocol.sendGetURLDataCmd( myID, str_url );

            while( !images.containsKey( str_url ) && active )
        {
                try { Thread.sleep( 200 ); }
                catch( InterruptedException e ){}
            }
            if( images.containsKey( str_url ) )
            {
                byte[] data = (byte[]) images.get( str_url );
                if( data.length > 0 )
                {
            Toolkit kit = Toolkit.getDefaultToolkit();
                    return kit.createImage( data );
                } else return null;
            }
        }

        return null;
    }

    public void showDocument( URL url )
    {
        Main.debug( "showDocument, url = " + url );

        if( active && (url != null) )
        {
            Main.protocol.sendShowDocumentCmd( myID, url.toString()  );
        }
    }

    public void showDocument( URL url, String targetFrame )
    {
        Main.debug( "showDocument, url = " + url + " targetFrame = " + targetFrame );

        if( active && (url != null) && (targetFrame != null) )
        {
                Main.protocol.sendShowDocumentCmd( myID, url.toString(), targetFrame );
        }
    }

    public void showStatus( String message )
    {
        if( active && (message != null) )
        {
            Main.protocol.sendShowStatusCmd( myID, message );
        }
    }

}
