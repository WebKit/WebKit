package org.kde.kjas.server;

import java.io.*;
import java.util.*;
import java.awt.*;
import java.net.*;

/**
 * Encapsulates the KJAS protocol and manages the contexts
 *
 */
public class KJASProtocolHandler
{
    // Command codes- always need to be synced up with
    // what's in kjavaappletserver.cpp
    private static final int CreateContextCode   = 1;
    private static final int DestroyContextCode  = 2;
    private static final int CreateAppletCode    = 3;
    private static final int DestroyAppletCode   = 4;
    private static final int StartAppletCode     = 5;
    private static final int StopAppletCode      = 6;
    private static final int InitAppletCode      = 7;
    private static final int ShowDocumentCode    = 8;
    private static final int ShowURLInFrameCode  = 9;
    private static final int ShowStatusCode      = 10;
    private static final int ResizeAppletCode    = 11;
    private static final int GetURLDataCode      = 12;
    private static final int URLDataCode         = 13;
    private static final int ShutdownServerCode  = 14;

    //Holds contexts in contextID-context pairs
    private Hashtable contexts;

    private PushbackInputStream commands;    //Stream for reading in commands
    private PrintStream         signals;     //Stream for writing out callbacks

    //used for parsing each command as it comes in
    private int cmd_index;
    private final char sep = (char) 0;

    public KJASProtocolHandler( InputStream  _commands,
                                OutputStream _signals )
    {
        commands = new PushbackInputStream( _commands );
        signals  = new PrintStream( _signals );
        contexts = new Hashtable();
    }

    public void commandLoop()
    {
        try
        {
            while( true )
            {
                try
                {
                    int cmd_length = readPaddedLength( 8 );
                    Main.debug( "PH: cmd_length = " + cmd_length );

                    //We need to have this while loop since we're not guaranteed to get
                    //all the bytes we want back, especially with large jars
                    byte[] cmd_data = new byte[cmd_length];
                    int total_read = 0;
                    while( total_read < cmd_length )
                    {
                        int numread = commands.read( cmd_data, total_read, cmd_length-total_read );
                        Main.debug( "PH: read in " + numread + " bytes for command" );
                        total_read += numread;
                    }

                    //parse the rest of the command and execute it
                    processCommand( cmd_data );
                }
                catch( NumberFormatException e )
                {
                    Main.kjas_err( "Could not parse out message length", e );
                    System.exit( 1 );
                }
                catch( Throwable t )
                {
                    Main.debug( "commandLoop caught a throwable, still going" );
                    t.printStackTrace();
                }
            }
        }
        catch( Exception i )
        {
            Main.kjas_err( "commandLoop exited on exception: ", i );
            System.exit( 1 );
        }
    }

    public void processCommand( byte[] command )
    {
        // Sanity checks
        if ( command == null )
            return;

        //do all the parsing here and pass arguments as individual variables to the
        //handler functions
        int cmd_length = command.length;
        cmd_index = 0;

        int cmd_code_value = (int) command[cmd_index++];
        if( cmd_code_value == CreateContextCode )
        {
            //parse out contextID- 1 argument
            String contextID = getArg( command );
            Main.debug( "createContext, id = " + contextID );

            KJASAppletContext context = new KJASAppletContext( contextID );
            contexts.put( contextID, context );
        } else
        if( cmd_code_value == DestroyContextCode )
        {
            //parse out contextID- 1 argument
            String contextID = getArg( command );
            Main.debug( "destroyContext, id = " + contextID );

            KJASAppletContext context = (KJASAppletContext) contexts.get( contextID );
            if( contexts != null )
            {
                context.destroy();
                contexts.remove( contextID );
            }
        } else
        if( cmd_code_value == CreateAppletCode )
        {
            //9 arguments- this order is important...
            final String contextID  = getArg( command );
            final String appletID   = getArg( command );
            final String appletName = getArg( command );
            final String className  = getArg( command );
            final String baseURL    = getArg( command );
            final String codeBase   = getArg( command );
            final String archives   = getArg( command );
            final String width      = getArg( command );
            final String height     = getArg( command );
            final String title      = getArg( command );

            //get the number of parameter pairs...
            String str_params = getArg( command );
            int num_params = Integer.parseInt( str_params.trim() );
            final Hashtable params = new Hashtable();
            for( int i = 0; i < num_params; i++ )
            {
                String name  = getArg( command );
                if( name == null )
                    name = new String();

                String value = getArg( command );
                if( value == null )
                    value = new String();
                params.put( name.toUpperCase(), value );
                //Main.debug( "parameter, name = " + name + ", value = " + value );
            }

            Main.debug( "createApplet, context = " + contextID + ", applet = " + appletID );
            Main.debug( "              name = " + appletName + ", classname = " + className );
            Main.debug( "              baseURL = " + baseURL + ", codeBase = " + codeBase );
            Main.debug( "              archives = " + archives + ", width = " + width + 
                        ", height = " + height );

            final KJASAppletContext context = (KJASAppletContext) contexts.get( contextID );
            if( context != null )
            {
                context.createApplet( appletID, appletName, className,
                                      baseURL, codeBase, archives,
                                      width, height, title, params );
            }

        } else
        if( cmd_code_value == DestroyAppletCode )
        {
            //2 arguments
            String contextID = getArg( command );
            String appletID  = getArg( command );
            Main.debug( "destroyApplet, context = " + contextID + ", applet = " + appletID );

            KJASAppletContext context = (KJASAppletContext) contexts.get( contextID );
            if ( context != null )
                context.destroyApplet( appletID );
        } else
        if( cmd_code_value == StartAppletCode )
        {
            //2 arguments
            String contextID = getArg( command );
            String appletID  = getArg( command );
            Main.debug( "startApplet, context = " + contextID + ", applet = " + appletID );

            KJASAppletContext context = (KJASAppletContext) contexts.get( contextID );
            if ( context != null )
                context.startApplet( appletID );
        } else
        if( cmd_code_value == StopAppletCode )
        {
            //2 arguments
            String contextID = getArg( command );
            String appletID  = getArg( command );
            Main.debug( "stopApplet, context = " + contextID + ", applet = " + appletID );

            KJASAppletContext context = (KJASAppletContext) contexts.get( contextID );
            if ( context != null )
                context.stopApplet( appletID );
        } else
        if( cmd_code_value == ShutdownServerCode )
        {
            Main.debug( "shutDownServer recieved" );
            System.exit( 1 );
        }
        else
        if( cmd_code_value == URLDataCode )
        {
            Main.debug( "URLData recieved" );
            
            String loaderID = getArg( command );
            String requestedURL = getArg( command );
            Main.debug( "data is for loader: " + loaderID );
            Main.debug( "URL is " + requestedURL );

            //rest of the command should be the data...
            byte[] data = new byte[ cmd_length - cmd_index ];
            System.arraycopy( command, cmd_index, data, 0, data.length );

            KJASAppletClassLoader loader = KJASAppletClassLoader.getLoader( loaderID );
            if( loader != null )
            {
                Main.debug( "this is a class loader request" );
                loader.addResource( requestedURL, data );
            }
            else //see if there is a context with that ID, could be an image request
            {
                KJASAppletContext context = (KJASAppletContext) contexts.get( loaderID );
                if( context != null )
                {
                    Main.debug( "this is  a context request for an image" );
                    context.addImage( requestedURL, data );
                }
            }
        }
        else
        {
           throw new IllegalArgumentException( "Unknown command code" );
        }
    }

    /**************************************************************
     *****  Methods for talking to the applet server **************
     **************************************************************/
    public void sendGetURLDataCmd( String loaderID, String file )
    {
        Main.debug( "sendGetURLCmd from loader: " + loaderID + " url = " + file );
        String ID_str = null;
        String file_str = null;
        try
        {
            ID_str = loaderID;
            file_str = new URL( new URL(loaderID), file ).toString();
        } catch( MalformedURLException e ) 
        {
            //this is an image request, take the file argument as is
            ID_str = loaderID;
            file_str = file;
        }
        finally
        {
            //length  = length of args plus 1 for code, 2 for seps and 1 for end
            int length = ID_str.length() + file_str.length() + 4;
            char[] chars = new char[ length + 8 ];
            char[] tmpchar = getPaddedLength( length );
            int index = 0;

            System.arraycopy( tmpchar, 0, chars, index, tmpchar.length );
            index += tmpchar.length;
            chars[index++] = (char) GetURLDataCode;
            chars[index++] = sep;

                tmpchar = ID_str.toCharArray();
            System.arraycopy( tmpchar, 0, chars, index, tmpchar.length );
            index += tmpchar.length;
            chars[index++] = sep;

                tmpchar = file_str.toCharArray();
            System.arraycopy( tmpchar, 0, chars, index, tmpchar.length );
            index += tmpchar.length;
            chars[index++] = sep;

            signals.print( chars );
        }
    }

    public void sendShowDocumentCmd( String loaderKey, String url )
    {
        Main.debug( "sendShowDocumentCmd from context#" + loaderKey + " url = " + url );

        //length = length of args + 2 for seps + 1 for end + 1 for code
        int length = loaderKey.length() + url.length() + 4;
        char[] chars = new char[ length + 8 ]; //8 for the length of this message
        char[] tmpchar = getPaddedLength( length );
        int index = 0;

        System.arraycopy( tmpchar, 0, chars, index, tmpchar.length );
        index += tmpchar.length;
        chars[index++] = (char) ShowDocumentCode;
        chars[index++] = sep;

        tmpchar = loaderKey.toCharArray();
        System.arraycopy( tmpchar, 0, chars, index, tmpchar.length );
        index += tmpchar.length;
        chars[index++] = sep;

        tmpchar = url.toCharArray();
        System.arraycopy( tmpchar, 0, chars, index, tmpchar.length );
        index += tmpchar.length;
        chars[index++] = sep;

        signals.print( chars );
    }

    public void sendShowDocumentCmd( String contextID, String url, String frame)
    {
        Main.debug( "sendShowDocumentCmd from context#" + contextID +
                         " url = " + url + ", frame = " + frame );

        //length = length of args plus code, 3 seps, end
        int length = contextID.length() + url.length() + frame.length() + 5;
        char[] chars = new char[ length + 8 ]; //for length of message
        char[] tmpchar = getPaddedLength( length );
        int index = 0;

        System.arraycopy( tmpchar, 0, chars, index, tmpchar.length );
        index += tmpchar.length;
        chars[index++] = (char) ShowURLInFrameCode;
        chars[index++] = sep;

        tmpchar = contextID.toCharArray();
        System.arraycopy( tmpchar, 0, chars, index, tmpchar.length );
        index += tmpchar.length;
        chars[index++] = sep;

        tmpchar = url.toCharArray();
        System.arraycopy( tmpchar, 0, chars, index, tmpchar.length );
        index += tmpchar.length;
        chars[index++] = sep;

        tmpchar = frame.toCharArray();
        System.arraycopy( tmpchar, 0, chars, index, tmpchar.length );
        index += tmpchar.length;
        chars[index++] = sep;

        signals.print( chars );
    }

    public void sendShowStatusCmd( String contextID, String msg )
    {
        Main.debug( "sendShowStatusCmd, contextID = " + contextID + " msg = " + msg );

        int length = contextID.length() + msg.length() + 4;
        char[] chars = new char[ length + 8 ]; //for length of message
        char[] tmpchar = getPaddedLength( length );
        int index = 0;

        System.arraycopy( tmpchar, 0, chars, index, tmpchar.length );
        index += tmpchar.length;
        chars[index++] = (char) ShowStatusCode;
        chars[index++] = sep;

        tmpchar = contextID.toCharArray();
        System.arraycopy( tmpchar, 0, chars, index, tmpchar.length );
        index += tmpchar.length;
        chars[index++] = sep;

        tmpchar = msg.toCharArray();
        System.arraycopy( tmpchar, 0, chars, index, tmpchar.length );
        index += tmpchar.length;
        chars[index++] = sep;

        signals.print( chars );
    }

    public void sendResizeAppletCmd( String contextID, String appletID,
                                     int width, int height )
    {
        Main.debug( "sendResizeAppletCmd, contextID = " + contextID + ", appletID = " + 
                    appletID + ", width = " + width + ", height = " + height );

        String width_str = String.valueOf( width );
        String height_str = String.valueOf( height );

        //lenght = length of args plus code, 4 seps, end
        int length = contextID.length() + appletID.length() + width_str.length() +
                     height_str.length() + 6;
        char[] chars = new char[ length + 8 ]; //for length of message
        char[] tmpchar = getPaddedLength( length );
        int index = 0;

        System.arraycopy( tmpchar, 0, chars, index, tmpchar.length );
        index += tmpchar.length;
        chars[index++] = (char) ShowStatusCode;
        chars[index++] = sep;

        tmpchar = contextID.toCharArray();
        System.arraycopy( tmpchar, 0, chars, index, tmpchar.length );
        index += tmpchar.length;
        chars[index++] = sep;

        tmpchar = appletID.toCharArray();
        System.arraycopy( tmpchar, 0, chars, index, tmpchar.length );
        index += tmpchar.length;
        chars[index++] = sep;

        tmpchar = width_str.toCharArray();
        System.arraycopy( tmpchar, 0, chars, index, tmpchar.length );
        index += tmpchar.length;
        chars[index++] = sep;

        tmpchar = height_str.toCharArray();
        System.arraycopy( tmpchar, 0, chars, index, tmpchar.length );
        index += tmpchar.length;
        chars[index++] = sep;

        signals.print( chars );
    }

    /**************************************************************
     *****  Utility functions for parsing commands ****************
     **************************************************************/
    private String getArg( byte[] command )
    {
        int begin = cmd_index;
        while( 0 != ((int) command[cmd_index++]) );

        if( cmd_index > (begin + 1) )
        {
            String rval = new String( command, begin, (cmd_index - begin - 1) );
            return rval;
        }
        else
            return null;
    }

    private char[] getPaddedLength( int length )
    {
        String length_str = String.valueOf( length );

        int pads = 8 - length_str.length();
        String space = new String( " " );
        String rval = length_str;
        for( int i = 0; i < pads; i++ )
        {
            rval = space.concat( rval );
        }

        if( rval.length() != 8 )
        {
           throw new IllegalArgumentException( "can't create string number of length = 8" );
        }

        return rval.toCharArray();
    }

    private int readPaddedLength( int string_size )
        throws IOException
    {
            //read in 8 bytes for command length- length will be sent as a padded string
            byte[] length = new byte[string_size];
            commands.read( length, 0, string_size );

            String length_str = new String( length );
            return Integer.parseInt( length_str.trim() );
    }

}
