#include "kjavaappletserver.h"
#include "kjavaappletcontext.h"
#include "kjavaprocess.h"
#include "kjavadownloader.h"

#include <kconfig.h>
#include <kstddirs.h>
#include <kdebug.h>
#include <klocale.h>
#include <kio/kprotocolmanager.h>
#include <kio/job.h>

#include <qtimer.h>
#include <qguardedptr.h>
#include <qdir.h>

#include <stdlib.h>

#define KJAS_CREATE_CONTEXT    (char)1
#define KJAS_DESTROY_CONTEXT   (char)2
#define KJAS_CREATE_APPLET     (char)3
#define KJAS_DESTROY_APPLET    (char)4
#define KJAS_START_APPLET      (char)5
#define KJAS_STOP_APPLET       (char)6
#define KJAS_INIT_APPLET       (char)7
#define KJAS_SHOW_DOCUMENT     (char)8
#define KJAS_SHOW_URLINFRAME   (char)9
#define KJAS_SHOW_STATUS       (char)10
#define KJAS_RESIZE_APPLET     (char)11
#define KJAS_GET_URLDATA       (char)12
#define KJAS_URLDATA           (char)13
#define KJAS_SHUTDOWN_SERVER   (char)14

// For future expansion
class KJavaAppletServerPrivate
{
friend class KJavaAppletServer;
private:
   int counter;
   QMap< int, QGuardedPtr<KJavaAppletContext> > contexts;
   QString appletLabel;
};

static KJavaAppletServer* self = 0;

KJavaAppletServer::KJavaAppletServer()
{
    d = new KJavaAppletServerPrivate;
    process = new KJavaProcess();

    connect( process, SIGNAL(received(const QByteArray&)),
             this,    SLOT(slotJavaRequest(const QByteArray&)) );

    setupJava( process );

    if( process->startJava() )
        d->appletLabel = i18n( "Loading Applet" );
    else
        d->appletLabel = i18n( "Error: java executable not found" );

}

KJavaAppletServer::~KJavaAppletServer()
{
    quit();

    delete process;
    delete d;
}

QString KJavaAppletServer::getAppletLabel()
{
    if( self )
        return self->appletLabel();
    else
        return QString::null;
}

QString KJavaAppletServer::appletLabel()
{
    return d->appletLabel;
}

KJavaAppletServer* KJavaAppletServer::allocateJavaServer()
{
   if( self == 0 )
   {
      self = new KJavaAppletServer();
      self->d->counter = 0;
   }

   self->d->counter++;
   return self;
}

void KJavaAppletServer::freeJavaServer()
{
    self->d->counter--;

    if( self->d->counter == 0 )
    {
        //instead of immediately quitting here, set a timer to kill us
        //if there are still no servers- give us one minute
        //this is to prevent repeated loading and unloading of the jvm
        KConfig config( "konquerorrc", true );
        config.setGroup( "Java/JavaScript Settings" );
        if( config.readBoolEntry( "ShutdownAppletServer", true )  )
        {
            int value = config.readNumEntry( "AppletServerTimeout", 60 );
            QTimer::singleShot( value*1000, self, SLOT( checkShutdown() ) );
        }
    }
}

void KJavaAppletServer::checkShutdown()
{
    if( self->d->counter == 0 )
    {
        delete self;
        self = 0;
    }
}

void KJavaAppletServer::setupJava( KJavaProcess *p )
{
    KConfig config ( "konquerorrc", true );
    config.setGroup( "Java/JavaScript Settings" );

    QString jvm_path = "java";

    QString jPath = config.readEntry( "JavaPath" );
    if ( !jPath.isEmpty() && jPath != "java" )
    {
        // Cut off trailing slash if any
        if( jPath[jPath.length()-1] == '/' )
            jPath.remove(jPath.length()-1, 1);

        QDir dir( jPath );
        if( dir.exists( "bin/java" ) )
            jvm_path = jPath + "/bin/java";
        else if( QFile::exists(jPath) ) //check here to see if they entered the whole path the java exe
            jvm_path = jPath;
    }

    //check to see if jvm_path is valid and set d->appletLabel accordingly
    p->setJVMPath( jvm_path );

    // Prepare classpath variable
    QString kjava_class = locate("data", "kjava/kjava.jar");
    kdDebug(6100) << "kjava_class = " << kjava_class << endl;
    if( kjava_class.isNull() ) // Should not happen
        return;

    QDir dir( kjava_class );
    dir.cdUp();
    kdDebug(6100) << "dir = " << dir.absPath() << endl;

    QStringList entries = dir.entryList( "*.jar" );
    kdDebug(6100) << "entries = " << entries.join( ":" ) << endl;

    QString classes;
    for( QStringList::Iterator it = entries.begin();
         it != entries.end(); it++ )
    {
        if( !classes.isEmpty() )
            classes += ":";
        classes += dir.absFilePath( *it );
    }
    p->setClasspath( classes );

    // Fix all the extra arguments
    QString extraArgs = config.readEntry( "JavaArgs", "" );
    p->setExtraArgs( extraArgs );

    if( config.readBoolEntry( "ShowJavaConsole", false) )
    {
        p->setSystemProperty( "kjas.showConsole", QString::null );
    }

    if( config.readBoolEntry( "UseSecurityManager", true ) )
    {
        QString class_file = locate( "data", "kjava/kjava.policy" );
        p->setSystemProperty( "java.security.policy", class_file );

        p->setSystemProperty( "java.security.manager",
                              "org.kde.kjas.server.KJASSecurityManager" );
    }

    //check for http proxies...
    if( KProtocolManager::useProxy() )
    {
        QString httpProxy = KProtocolManager::httpProxy();
        kdDebug(6100) << "httpProxy is " << httpProxy << endl;

        KURL url( httpProxy );
        p->setSystemProperty( "http.proxyHost", url.host() );
        p->setSystemProperty( "http.proxyPort", QString::number( url.port() ) );
    }

    //set the main class to run
    p->setMainClass( "org.kde.kjas.server.Main" );
}

void KJavaAppletServer::createContext( int contextId, KJavaAppletContext* context )
{
//    kdDebug(6100) << "createContext: " << contextId << endl;
    d->contexts.insert( contextId, context );

    QStringList args;
    args.append( QString::number( contextId ) );
    process->send( KJAS_CREATE_CONTEXT, args );
}

void KJavaAppletServer::destroyContext( int contextId )
{
//    kdDebug(6100) << "destroyContext: " << contextId << endl;
    d->contexts.remove( contextId );

    QStringList args;
    args.append( QString::number( contextId ) );
    process->send( KJAS_DESTROY_CONTEXT, args );
}

void KJavaAppletServer::createApplet( int contextId, int appletId,
                                      const QString name, const QString clazzName,
                                      const QString baseURL, const QString codeBase,
                                      const QString jarFile, QSize size,
                                      const QMap<QString,QString>& params,
                                      const QString windowTitle )
{
//    kdDebug(6100) << "createApplet: contextId = " << contextId     << endl
//              << "              appletId  = " << appletId      << endl
//              << "              name      = " << name          << endl
//              << "              clazzName = " << clazzName     << endl
//              << "              baseURL   = " << baseURL       << endl
//              << "              codeBase  = " << codeBase      << endl
//              << "              jarFile   = " << jarFile       << endl
//              << "              width     = " << size.width()  << endl
//              << "              height    = " << size.height() << endl;

    QStringList args;
    args.append( QString::number( contextId ) );
    args.append( QString::number( appletId ) );

    //it's ok if these are empty strings, I take care of it later...
    args.append( name );
    args.append( clazzName );
    args.append( baseURL );
    args.append( codeBase );
    args.append( jarFile );

    args.append( QString::number( size.width() ) );
    args.append( QString::number( size.height() ) );

    args.append( windowTitle );

    //add on the number of parameter pairs...
    int num = params.count();
    QString num_params = QString("%1").arg( num, 8 );
    args.append( num_params );

    QMap< QString, QString >::ConstIterator it;

    for( it = params.begin(); it != params.end(); ++it )
    {
        args.append( it.key() );
        args.append( it.data() );
    }

    process->send( KJAS_CREATE_APPLET, args );
}

void KJavaAppletServer::initApplet( int contextId, int appletId )
{
    QStringList args;
    args.append( QString::number( contextId ) );
    args.append( QString::number( appletId ) );

    process->send( KJAS_INIT_APPLET, args );
}

void KJavaAppletServer::destroyApplet( int contextId, int appletId )
{
    QStringList args;
    args.append( QString::number(contextId) );
    args.append( QString::number(appletId) );

    process->send( KJAS_DESTROY_APPLET, args );
}

void KJavaAppletServer::startApplet( int contextId, int appletId )
{
    QStringList args;
    args.append( QString::number(contextId) );
    args.append( QString::number(appletId) );

    process->send( KJAS_START_APPLET, args );
}

void KJavaAppletServer::stopApplet( int contextId, int appletId )
{
    QStringList args;
    args.append( QString::number(contextId) );
    args.append( QString::number(appletId) );

    process->send( KJAS_STOP_APPLET, args );
}

void KJavaAppletServer::sendURLData( const QString& loaderID,
                                     const QString& url,
                                     const QByteArray& data )
{
    QStringList args;
    args.append( loaderID );
    args.append( url );

    process->send( KJAS_URLDATA, args, data );

}

void KJavaAppletServer::quit()
{
    QStringList args;

    process->send( KJAS_SHUTDOWN_SERVER, args );
}

void KJavaAppletServer::slotJavaRequest( const QByteArray& qb )
{
    // qb should be one command only without the length string,
    // we parse out the command and it's meaning here...
    QString cmd;
    QStringList args;
    int index = 0;
    int qb_size = qb.size();

    //get the command code
    char cmd_code = qb[ index++ ];
    ++index; //skip the next sep

    //get contextID
    QString contextID;
    while( qb[index] != 0 && index < qb_size )
    {
        contextID += qb[ index++ ];
    }
    ++index; //skip the sep

    //now parse out the arguments
    while( index < qb_size )
    {
        QString tmp;
        while( qb[index] != 0 )
            tmp += qb[ index++ ];

        args.append( tmp );

        ++index; //skip the sep
    }

    //here I should find the context and call the method directly
    //instead of emitting signals
    switch( cmd_code )
    {
        case KJAS_SHOW_DOCUMENT:
            cmd = QString::fromLatin1( "showdocument" );
            break;

        case KJAS_SHOW_URLINFRAME:
            cmd = QString::fromLatin1( "showurlinframe" );
            break;

        case KJAS_SHOW_STATUS:
            cmd = QString::fromLatin1( "showstatus" );
            break;

        case KJAS_RESIZE_APPLET:
            cmd = QString::fromLatin1( "resizeapplet" );
            break;

        case KJAS_GET_URLDATA:
            //here we need to get some data for a class loader and send it back...
            kdDebug(6100) << "GetURLData from classloader: "<< contextID
                          << " for url: " << args[0] << endl;
            break;

        default:
            return;
            break;
    }

    if( cmd_code == KJAS_GET_URLDATA )
    {
        new KJavaDownloader( contextID, args[0] );
    }
    else
    {
        bool ok;
        int contextID_num = contextID.toInt( &ok );

        if( !ok )
        {
            kdError(6100) << "could not parse out contextID to call command on" << endl;
            return;
        }

        KJavaAppletContext* tmp = d->contexts[ contextID_num ];
        if( tmp )
            tmp->processCmd( cmd, args );
        else
            kdError(6100) << "no context object for this id" << endl;
    }
}

#include "kjavaappletserver.moc"
