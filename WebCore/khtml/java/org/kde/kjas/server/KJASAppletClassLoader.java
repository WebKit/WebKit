package org.kde.kjas.server;

import java.net.*;
import java.io.*;
import java.util.*;
import java.util.zip.*;
import java.util.jar.*;
import java.security.*;


/**
 * ClassLoader used to download and instantiate Applets.
 * <P>
 * NOTE: The class loader extends Java 1.2 specific class.
 */
public class KJASAppletClassLoader
    extends SecureClassLoader
{
    private static Hashtable loaders = new Hashtable();
    public static KJASAppletClassLoader getLoader( String docBase, String codeBase )
    {
        URL docBaseURL;
        KJASAppletClassLoader loader = null;
        try
        {
            docBaseURL = new URL( docBase );
        
            URL key = getCodeBaseURL( docBaseURL, codeBase );
            Main.debug( "CL: getLoader: key = " + key );

            loader = (KJASAppletClassLoader) loaders.get( key.toString() );
            if( loader == null )
            {
                loader = new KJASAppletClassLoader( docBaseURL, key );
                loaders.put( key.toString(), loader );
            }
            else
            {
                Main.debug( "CL: reusing classloader" );
                loader.setActive();
            }
        } catch( MalformedURLException e ) { Main.kjas_err( "bad DocBase URL", e ); }
        return loader;
    }

    public static URL getCodeBaseURL( URL docBaseURL, String codeBase )
    {
        URL codeBaseURL = null;
        try
        {
            //first determine what the real codeBase is: 3 cases
            //#1. codeBase is absolute URL- use that
            //#2. codeBase is relative to docBase, create url from those
            //#3. last resort, use docBase as the codeBase
            if(codeBase != null)
            {
                //we need to do this since codeBase should be a directory
                if( !codeBase.endsWith("/") )
                    codeBase = codeBase + "/";

                try
                {
                    codeBaseURL = new URL( codeBase );
                } catch( MalformedURLException mue )
                {
                    try
                    {
                        codeBaseURL = new URL( docBaseURL, codeBase );
                    } catch( MalformedURLException mue2 ) {}
                }
            }

            if(codeBaseURL == null)
            {
                //fall back to docBase but fix it up...
                String file = docBaseURL.getFile();
                if( file == null || (file.length() == 0)  )
                    codeBaseURL = docBaseURL;
                else if( file.endsWith( "/" ) )
                    codeBaseURL = docBaseURL;
                else
                {
                    //delete up to the ending '/'
                    String urlString = docBaseURL.toString();
                    int dot_index = urlString.lastIndexOf( '/' );
                    String newfile = urlString.substring( 0, dot_index+1 );
                    codeBaseURL = new URL( newfile );
                }
            }
        }catch( Exception e ) { Main.kjas_err( "CL: exception ", e ); }
        return codeBaseURL;    
    }

    public static KJASAppletClassLoader getLoader( String key )
    {
        if( loaders.containsKey( key ) )
            return (KJASAppletClassLoader) loaders.get( key );
        
        return null;
    }

    /*********************************************************************************
     ****************** KJASAppletClassLoader Implementation *************************
     **********************************************************************************/
    private URL docBaseURL;
    private URL codeBaseURL;
    private Vector archives;
    private Hashtable rawdata;
    private Hashtable certificates;
    private boolean archives_loaded;
    private int archive_count;
    private String dbgID;
    private boolean active;
    
    public KJASAppletClassLoader( URL _docBaseURL, URL _codeBaseURL )
    {
        docBaseURL   = _docBaseURL;
        codeBaseURL  = _codeBaseURL;
        archives     = new Vector();
        rawdata      = new Hashtable();
        certificates = new Hashtable();
        
        archives_loaded = false;
        archive_count   = 0;
        active          = true;
        
        dbgID = "CL(" + codeBaseURL.toString() + "): ";
    }

    public void setActive()
    {
        active = true;
    }

    public void setInactive()
    {
        active = false;
    }

    public void paramsDone()
    {
        //if we have archives, send download requests
        if( archives.size() > 0 )
        {
            if( !archives_loaded )
            {
                for( int i = 0; i < archives.size(); ++i )
                {
                    String tmp = (String)archives.elementAt( i );
                    Main.protocol.sendGetURLDataCmd( codeBaseURL.toString(), tmp );
                }
            }
        }
        else archives_loaded = true;
    }

    public void addArchiveName( String jarname )
    {
        if( !archives.contains( jarname ) )
        {
            archives.add( jarname );
            archives_loaded = false;
        }
    }
    
    public void addResource( String url, byte[] data )
    {
        Main.debug( dbgID + "addResource for url: " + url + ", size of data = " + data.length );
        String res = url.substring( codeBaseURL.toString().length() ).replace( '/', '.' );

        if( archives.size() > 0 && !res.endsWith( ".class" ) )
        {   //if we have archives, it's an archive( i think )
        try
            {
                JarInputStream jar = new JarInputStream( new ByteArrayInputStream( data ) );
                JarEntry entry;
                while( (entry = jar.getNextJarEntry()) != null )
                {
                    //skip directories...
                    if( entry.isDirectory() )
                        continue;

                    String entryName = entry.getName().replace('/','.');
                    int    entrySize = (int) entry.getSize();
                    Main.debug( dbgID + "reading ZipEntry, name = " + entryName + ", size = " + entrySize );

                    int numread = 0;
                    int total = 0;
                    byte[] entryData = new byte[0];
                    while( numread > -1 )
        {
                        byte[] curr = new byte[1024];
                        numread = jar.read( curr, 0, 1024 );
                        if( numread == -1 )
                            break;

                        byte[] old = entryData;
                        entryData = new byte[ old.length + numread];
                       // Main.debug( "old.length = " + old.length );
                        if( old.length > 0 )
                            System.arraycopy( old, 0, entryData, 0, old.length );
                        System.arraycopy( curr, 0, entryData, old.length, numread );

                        total += numread;
                    }

                    byte[] old = entryData;
                    entryData = new byte[ total ];
                    System.arraycopy( old, 0, entryData, 0, total );
                    rawdata.put( entryName, entryData );

                    java.security.cert.Certificate[] c = entry.getCertificates();
                    if( c == null )
                    {
                        c = new java.security.cert.Certificate[0];
                        Main.debug( "making a dummy certificate array" );
                    } else Main.debug( "got some real certificates with archive" );
                    certificates.put( entryName, c );
        }
            }
            catch( IOException e )
        {
                Main.kjas_err( "Problem reading resource", e );
        }
            finally
            {
                if( (++archive_count) == archives.size() )
                {
                    Main.debug( dbgID + "all archives loaded" );
                    archives_loaded = true;
    }
            }
        }
        else
    {
            String resName = url.substring( codeBaseURL.toString().length() ).replace( '/', '.' );
            Main.debug( dbgID + "resource isn't a jar, putting it straight in with name = " + resName );
            rawdata.put( resName, data );
    }
    }

    public URL getDocBase()
    {
        return docBaseURL;
    }

    public URL getCodeBase()
    {
        return codeBaseURL;
    }

    /***************************************************************************
     **** Class Loading Methods
     **************************************************************************/
    public Class findClass( String name )
    {
        Class rval;
        
        try
        {
            //check for a system class
            rval = findSystemClass( name );
            if( rval != null )
                return rval;
        } catch (ClassNotFoundException e )
        {
            //check the loaded classes 
            rval = findLoadedClass( name );
            if( rval != null )
                return rval;

            //check in the archives
            String fixed_name = name + ".class";
            while( !archives_loaded && active )
            {
                Main.debug( dbgID + "archives not loaded yet, sleeping" );
                try { Thread.sleep( 200 ); }
                catch( InterruptedException te ) {}
            }
            if( rawdata.containsKey( fixed_name ) )
            {
                Main.debug( dbgID + "class is in our rawdata table" );
                byte[] data = (byte[]) rawdata.get( fixed_name );
                if( data.length > 0 )
                {
                    java.security.cert.Certificate[] c = 
                        (java.security.cert.Certificate[])certificates.get( fixed_name );
                    CodeSource cs = new CodeSource( codeBaseURL, c );
                    rval = defineClass( name, data, 0, data.length, cs );
                    return rval;
                } else return null;
            }

            //check from the webserver...
            Main.debug( dbgID + "now checking the webserver" );
            String new_name = name.replace( '.', '/' );
            new_name += ".class";
            Main.protocol.sendGetURLDataCmd( codeBaseURL.toString(), new_name );

            //now wait until we get an answer
            while( !rawdata.containsKey( fixed_name ) && active )
            {
                Main.debug( dbgID + "waiting for the webserver to answer for class: " + new_name );
                try { Thread.sleep( 200 ); } 
                catch( InterruptedException ie ) {}
            }
            if( rawdata.containsKey( fixed_name ) )
            {
                byte[] data = (byte[]) rawdata.get( fixed_name );
                if( data.length > 0 )
            {
                    Main.debug( "we got the data" );
                    CodeSource cs = new CodeSource( codeBaseURL, new java.security.cert.Certificate[0] );
                    rval = defineClass( name, data, 0, data.length, cs );
                    return rval;
                } else return null;
            }
        }
        
        Main.debug( "CL: findClass returning null" );
            return null;
    }
    
    public Class loadClass( String name )
    {
        Main.debug( dbgID + "loadClass, class name = " + name );
        //We need to be able to handle foo.class, so strip off the suffix
        String fixed_name = name;
        Class rval = null;
        if( name.endsWith( ".class" ) )
        {
            fixed_name = name.substring( 0, name.lastIndexOf( ".class" ) );
        }

        rval = findClass( fixed_name );
        return rval;
    }

    public InputStream getResourceAsStream( String name )
    {
        Main.debug( dbgID + "getResourceAsStream, name = " + name );
        
        String res = name.replace( '/', '.' );
        if( rawdata.containsKey( res ) )
        {
            byte[] data = (byte[]) rawdata.get( res );
            if( data.length > 0 )
            {
                return new ByteArrayInputStream( data );
            } else return null;
        }
        
        return super.getResourceAsStream( name );
    }
    
    public URL getResource( String name )
    {
        Main.debug( dbgID + "getResource, name = " + name );
        return super.getResource( name );
    }
    
    /***************************************************************************
     * Security Manager stuff
     **************************************************************************/
    protected PermissionCollection getPermissions( CodeSource cs )
    {
        //get the permissions from the SecureClassLoader
        final PermissionCollection perms = super.getPermissions( cs );
        final URL url = cs.getLocation();

        //first add permission to connect back to originating host
        perms.add(new SocketPermission(url.getHost(), "connect,accept"));

        //add ability to read from it's own directory...
        if ( url.getProtocol().equals("file") )
        {
            String path = url.getFile().replace('/', File.separatorChar);

            if (!path.endsWith(File.separator))
            {
                int endIndex = path.lastIndexOf(File.separatorChar);
                if (endIndex != -1)
                {
                    path = path.substring(0, endIndex+1) + "-";
                    perms.add(new FilePermission(path, "read"));
                }
            }

            AccessController.doPrivileged(
                new PrivilegedAction()
                {
                    public Object run()
                    {
                        try
                        {
                            if (InetAddress.getLocalHost().equals(InetAddress.getByName(url.getHost())))
                            {
                                perms.add(new SocketPermission("localhost", "connect,accept"));
                            }
                        } catch (UnknownHostException uhe)
                        {}
                        return null;
                    }
                }
            );
        }

        return perms;
    }

    private void dump2File( String filename, byte[] data )
    {
        Main.debug( "dump2File: " + filename );
        try
        {
            FileOutputStream output = new FileOutputStream( filename );
            output.write( data );
            output.close();
        }catch( IOException e ){}
    }
}
