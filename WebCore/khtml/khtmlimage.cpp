/* This file is part of the KDE project
   Copyright (C) 2000 Simon Hausmann <hausmann@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include "khtmlimage.h"
#include "khtml_part.h"
#include "khtmlview.h"
#include "khtml_ext.h"
#include "xml/dom_docimpl.h"
#include "html/html_documentimpl.h"
#include "html/html_elementimpl.h"
#include "rendering/render_image.h"
#include "misc/loader.h"

#include <qvbox.h>
#include <qtimer.h>

#include <kparts/factory.h>
#include <kio/job.h>
#include <kglobal.h>
#include <kinstance.h>
#include <kaction.h>
#include <kmimetype.h>
#include <klocale.h>

extern "C"
{
    void *init_libkhtmlimage()
    {
        return new KHTMLImageFactory();
    }
};

KInstance *KHTMLImageFactory::s_instance = 0;

KHTMLImageFactory::KHTMLImageFactory()
{
    s_instance = new KInstance( "khtmlimage" );
}

KHTMLImageFactory::~KHTMLImageFactory()
{
    delete s_instance;
}

KParts::Part *KHTMLImageFactory::createPartObject( QWidget *parentWidget, const char *widgetName,
                                                   QObject *parent, const char *name,
                                                   const char *, const QStringList & )
{
    return new KHTMLImage( parentWidget, widgetName, parent, name );
}

KHTMLImage::KHTMLImage( QWidget *parentWidget, const char *widgetName,
                        QObject *parent, const char *name )
    : KParts::ReadOnlyPart( parent, name )
{
    setInstance( KHTMLImageFactory::instance() );

    QVBox *box = new QVBox( parentWidget, widgetName );

    m_khtml = new KHTMLPart( box, widgetName, this, "htmlimagepart" );
    m_khtml->autoloadImages( true );

    setWidget( box );

    setXMLFile( m_khtml->xmlFile() );

    m_ext = new KHTMLImageBrowserExtension( this, "be" );

    connect( m_khtml->browserExtension(), SIGNAL( popupMenu( KXMLGUIClient *, const QPoint &, const KURL &, const QString &, mode_t ) ),
             this, SLOT( slotPopupMenu( KXMLGUIClient *, const QPoint &, const KURL &, const QString &, mode_t ) ) );

    connect( m_khtml->browserExtension(), SIGNAL( enableAction( const char *, bool ) ),
             m_ext, SIGNAL( enableAction( const char *, bool ) ) );

    m_ext->setURLDropHandlingEnabled( true );
}

KHTMLImage::~KHTMLImage()
{
    // important: delete the html part before the part or qobject destructor runs.
    // we now delete the htmlpart which deletes the part's widget which makes
    // _OUR_ m_widget 0 which in turn avoids our part destructor to delete the
    // widget ;-)
    // ### additional note: it _can_ be that the part has been deleted before:
    // when we're in a html frameset and the view dies first, then it will also
    // kill the htmlpart
    if ( m_khtml )
        delete static_cast<KHTMLPart *>( m_khtml );
}

bool KHTMLImage::openURL( const KURL &url )
{
    static const QString &html = KGlobal::staticQString( "<html><body><img src=\"%1\"></body></html>" );

    m_url = url;

    emit started( 0 );

    KParts::URLArgs args = m_ext->urlArgs();
    m_mimeType = args.serviceType;

    m_khtml->begin( m_url, args.xOffset, args.yOffset );
    m_khtml->setAutoloadImages( true );

    DOM::DocumentImpl *impl = dynamic_cast<DOM::DocumentImpl *>( m_khtml->document().handle() ); // ### hack ;-)
    if ( impl && m_ext->urlArgs().reload )
        impl->docLoader()->setReloading(true);

    m_khtml->write( html.arg( m_url.url() ) );
    m_khtml->end();

    KIO::Job *job = khtml::Cache::loader()->jobForRequest( m_url.url() );

    emit setWindowCaption( url.prettyURL() );

    if ( job )
    {
        emit started( job );

        connect( job, SIGNAL( result( KIO::Job * ) ),
                 this, SLOT( slotImageJobFinished( KIO::Job * ) ) );
    }
    else
    {
        emit started( 0 );
        emit completed();
    }

    return true;
}

bool KHTMLImage::closeURL()
{
    return true;
}

void KHTMLImage::guiActivateEvent( KParts::GUIActivateEvent *e )
{
    if ( e->activated() )
        emit setWindowCaption( m_url.prettyURL() );
}

void KHTMLImage::slotPopupMenu( KXMLGUIClient *cl, const QPoint &pos, const KURL &u,
                                const QString &, mode_t mode )
{
    KAction *encodingAction = cl->actionCollection()->action( "setEncoding" );
    if ( encodingAction )
        cl->actionCollection()->take( encodingAction );
    emit m_ext->popupMenu( cl, pos, u, m_mimeType, mode );
}

void KHTMLImage::slotImageJobFinished( KIO::Job *job )
{
    if ( job->error() )
    {
        job->showErrorDialog();
        emit canceled( job->errorString() );
    }
    else
    {
        if ( m_khtml->view()->contentsY() == 0 )
        {
            KParts::URLArgs args = m_ext->urlArgs();
            m_khtml->view()->setContentsPos( args.xOffset, args.yOffset );
        }

        emit completed();

        QTimer::singleShot( 0, this, SLOT( updateWindowCaption() ) );
    }
}

void KHTMLImage::updateWindowCaption()
{
    if ( !m_khtml )
        return;

    DOM::HTMLDocumentImpl *impl = dynamic_cast<DOM::HTMLDocumentImpl *>( m_khtml->document().handle() );
    if ( !impl )
        return;

    DOM::HTMLElementImpl *body = impl->body();
    if ( !body )
        return;

    DOM::NodeImpl *image = body->firstChild();
    if ( !image )
        return;

    khtml::RenderImage *renderImage = dynamic_cast<khtml::RenderImage *>( image->renderer() );
    if ( !renderImage )
        return;

    QPixmap pix = renderImage->pixmap();

    QString caption;

    KMimeType::Ptr mimeType;
    if ( !m_mimeType.isEmpty() )
        mimeType = KMimeType::mimeType( m_mimeType );

    if ( mimeType )
        caption = i18n( "%1 - %2x%3 Pixels" ).arg( mimeType->comment() )
                  .arg( pix.width() ).arg( pix.height() );
    else
        caption = i18n( "Image - %2x%3 Pixels" ).arg( pix.width() ).arg( pix.height() );

    emit setWindowCaption( caption );
}

KHTMLImageBrowserExtension::KHTMLImageBrowserExtension( KHTMLImage *parent, const char *name )
    : KParts::BrowserExtension( parent, name )
{
    m_imgPart = parent;
}

int KHTMLImageBrowserExtension::xOffset()
{
    return m_imgPart->doc()->view()->contentsX();
}

int KHTMLImageBrowserExtension::yOffset()
{
    return m_imgPart->doc()->view()->contentsY();
}

void KHTMLImageBrowserExtension::print()
{
    static_cast<KHTMLPartBrowserExtension *>( m_imgPart->doc()->browserExtension() )->print();
}

void KHTMLImageBrowserExtension::reparseConfiguration()
{
    static_cast<KHTMLPartBrowserExtension *>( m_imgPart->doc()->browserExtension() )->reparseConfiguration();
    m_imgPart->doc()->autoloadImages( true );
}

using namespace KParts;

#include "khtmlimage.moc"
