#include "kjavaappletwidget.h"
#include "kjavaappletserver.h"

#include <kwin.h>
#include <kdebug.h>
#include <klocale.h>

#include <qlabel.h>

// For future expansion
class KJavaAppletWidgetPrivate
{
friend class KJavaAppletWidget;
private:
    QLabel* tmplabel;
};

int KJavaAppletWidget::appletCount = 0;

KJavaAppletWidget::KJavaAppletWidget( KJavaAppletContext* context,
                                      QWidget* parent, const char* name )
   : KJavaEmbed( parent, name )
{
    m_applet = new KJavaApplet( this, context );
    d        = new KJavaAppletWidgetPrivate;
    m_kwm    = new KWinModule( this );

    d->tmplabel = new QLabel( this );
    d->tmplabel->setText( KJavaAppletServer::getAppletLabel() );
    d->tmplabel->setAlignment( Qt::AlignCenter | Qt::WordBreak );
    d->tmplabel->setFrameStyle( QFrame::StyledPanel | QFrame::Sunken );
    d->tmplabel->show();

    m_swallowTitle.sprintf( "KJAS Applet - Ticket number %u", appletCount++ );
    m_applet->setWindowName( m_swallowTitle );
}

KJavaAppletWidget::~KJavaAppletWidget()
{
    delete m_applet;
    delete d;
}

void KJavaAppletWidget::showApplet()
{
    connect( m_kwm, SIGNAL( windowAdded( WId ) ),
	         this,  SLOT( setWindow( WId ) ) );
    m_kwm->doNotManage( m_swallowTitle );

    //Now we send applet info to the applet server
    if ( !m_applet->isCreated() )
        m_applet->create();
}

void KJavaAppletWidget::setWindow( WId w )
{
    //make sure that this window has the right name, if so, embed it...
    KWin::Info w_info = KWin::info( w );

    if ( m_swallowTitle == w_info.name ||
         m_swallowTitle == w_info.visibleName )
    {
        kdDebug(6100) << "swallowing our window: " << m_swallowTitle
                      << ", window id = " << w << endl;

        delete d->tmplabel;
        d->tmplabel = 0;

        // disconnect from KWM events
        disconnect( m_kwm, SIGNAL( windowAdded( WId ) ),
                    this,  SLOT( setWindow( WId ) ) );

        embed( w );
        setFocus();
    }
}

QSize KJavaAppletWidget::sizeHint()
{
    kdDebug(6100) << "KJavaAppletWidget::sizeHint()" << endl;
    QSize rval = KJavaEmbed::sizeHint();

    if( rval.width() == 0 || rval.height() == 0 )
    {
        if( width() != 0 && height() != 0 )
        {
            rval = QSize( width(), height() );
        }
    }

    kdDebug(6100) << "returning: (" << rval.width() << ", " << rval.height() << ")" << endl;

    return rval;
}

void KJavaAppletWidget::resize( int w, int h )
{
    kdDebug(6100) << "KJavaAppletWidget, id = " << m_applet->appletId() << ", ::resize to: " << w << ", " << h << endl;

    if( d->tmplabel )
    {
        d->tmplabel->resize( w, h );
        m_applet->setSize( QSize( w, h ) );
    }

    KJavaEmbed::resize( w, h );
}

#include "kjavaappletwidget.moc"
