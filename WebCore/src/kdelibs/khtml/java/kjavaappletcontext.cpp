#include "kjavaappletcontext.h"

#include <kjavaappletserver.h>
#include <kjavaapplet.h>
#include <kdebug.h>
#include <qmap.h>
#include <qguardedptr.h>

// For future expansion
class KJavaAppletContextPrivate
{
friend class KJavaAppletContext;
private:
    QMap< int, QGuardedPtr<KJavaApplet> > applets;
};


//  Static Factory Functions
int KJavaAppletContext::contextCount = 0;

/*  Class Implementation
 */
KJavaAppletContext::KJavaAppletContext()
    : QObject()
{
    d = new KJavaAppletContextPrivate;
    server = KJavaAppletServer::allocateJavaServer();

    id = contextCount;
    server->createContext( id, this );

    contextCount++;
}

KJavaAppletContext::~KJavaAppletContext()
{
    server->destroyContext( id );
    KJavaAppletServer::freeJavaServer();
    delete d;
}

int KJavaAppletContext::contextId()
{
    return id;
}

void KJavaAppletContext::setContextId( int _id )
{
    id = _id;
}

void KJavaAppletContext::create( KJavaApplet* applet )
{
    static int appletId = 0;

    server->createApplet( id, appletId,
                          applet->appletName(),
                          applet->appletClass(),
                          applet->baseURL(),
                          applet->codeBase(),
                          applet->archives(),
                          applet->size(),
                          applet->getParams(),
                          applet->getWindowName() );

    applet->setAppletId( appletId );
    d->applets.insert( appletId, applet );
    appletId++;
}

void KJavaAppletContext::destroy( KJavaApplet* applet )
{
    int appletId = applet->appletId();
    d->applets.remove( appletId );

    server->destroyApplet( id, appletId );
}

void KJavaAppletContext::init( KJavaApplet* applet )
{
    server->initApplet( id, applet->appletId() );
}

void KJavaAppletContext::start( KJavaApplet* applet )
{
    server->startApplet( id, applet->appletId() );
}

void KJavaAppletContext::stop( KJavaApplet* applet )
{
    server->stopApplet( id, applet->appletId() );
}

void KJavaAppletContext::processCmd( QString cmd, QStringList args )
{
    received( cmd, args );
}

void KJavaAppletContext::received( const QString& cmd, const QStringList& arg )
{
    kdDebug(6100) << "KJavaAppletContext::received, cmd = >>" << cmd << "<<" << endl;
    kdDebug(6100) << "arg count = " << arg.count() << endl;

    if ( cmd == QString::fromLatin1("showstatus")
         && arg.count() > 0 )
    {
        kdDebug(6100) << "status message = " << arg[0] << endl;
        emit showStatus( arg[0] );
    }
    else if ( cmd == QString::fromLatin1( "showurlinframe" )
              && arg.count() > 1 )
    {
        kdDebug(6100) << "url = " << arg[0] << ", frame = " << arg[1] << endl;
        emit showDocument( arg[0], arg[1] );
    }
    else if ( cmd == QString::fromLatin1( "showdocument" )
              && arg.count() > 0 )
    {
        kdDebug(6100) << "url = " << arg[0] << endl;
        emit showDocument( arg[0], "_top" );
    }
    else if ( cmd == QString::fromLatin1( "resizeapplet" )
              && arg.count() > 0 )
    {
        //arg[1] should be appletID
        //arg[2] should be new width
        //arg[3] should be new height
        bool ok;
        int appletID = arg[0].toInt( &ok );
        int width = arg[1].toInt( &ok );
        int height = arg[2].toInt( &ok );

        if( !ok )
        {
            kdError(6002) << "could not parse out parameters for resize" << endl;
        }
        else
        {
            KJavaApplet* tmp = d->applets[appletID];
            tmp->resizeAppletWidget( width, height );
        }
    }
}

#include <kjavaappletcontext.moc>
