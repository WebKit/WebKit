
#include "khtml_ext.h"
#include "khtmlview.h"
#include "khtml_pagecache.h"
#include "rendering/render_form.h"
#include "dom/html_form.h"
#include <qapplication.h>
#include <qclipboard.h>
#include <qpopupmenu.h>
#include <qlineedit.h>
#include <qmetaobject.h>

#include <kdebug.h>
#include <klocale.h>
#include <kfiledialog.h>
#include <kio/job.h>
#include <ktoolbarbutton.h>
#include <ktoolbar.h>
#include <ktempfile.h>
#include <ksavefile.h>
#include <kurldrag.h>
#include <kstringhandler.h>

#include <dom/dom_element.h>
#include <misc/htmltags.h>

KHTMLPartBrowserExtension::KHTMLPartBrowserExtension( KHTMLPart *parent, const char *name )
: KParts::BrowserExtension( parent, name )
{
    m_part = parent;
    setURLDropHandlingEnabled( true );

    enableAction( "cut", false );
    enableAction( "copy", false );
    enableAction( "paste", false );

    m_connectedToClipboard = false;
}

int KHTMLPartBrowserExtension::xOffset()
{
    return m_part->view()->contentsX();
}

int KHTMLPartBrowserExtension::yOffset()
{
  return m_part->view()->contentsY();
}

void KHTMLPartBrowserExtension::saveState( QDataStream &stream )
{
  kdDebug( 6050 ) << "saveState!" << endl;
  m_part->saveState( stream );
}

void KHTMLPartBrowserExtension::restoreState( QDataStream &stream )
{
  kdDebug( 6050 ) << "restoreState!" << endl;
  m_part->restoreState( stream );
}

void KHTMLPartBrowserExtension::editableWidgetFocused( QWidget *widget )
{
    m_editableFormWidget = widget;
    updateEditActions();

    if ( !m_connectedToClipboard && m_editableFormWidget )
    {
        connect( QApplication::clipboard(), SIGNAL( dataChanged() ),
                 this, SLOT( updateEditActions() ) );

        if ( m_editableFormWidget->inherits( "QLineEdit" ) )
            connect( m_editableFormWidget, SIGNAL( textChanged( const QString & ) ),
                     this, SLOT( updateEditActions() ) );
        else if ( m_editableFormWidget->inherits( "QMultiLineEdit" ) )
            connect( m_editableFormWidget, SIGNAL( textChanged() ),
                     this, SLOT( updateEditActions() ) );

        m_connectedToClipboard = true;
    }
}

void KHTMLPartBrowserExtension::editableWidgetBlurred( QWidget *widget )
{
    QWidget *oldWidget = m_editableFormWidget;

    m_editableFormWidget = widget;
    enableAction( "cut", false );
    enableAction( "paste", false );
    m_part->emitSelectionChanged();

    if ( m_connectedToClipboard )
    {
        disconnect( QApplication::clipboard(), SIGNAL( dataChanged() ),
                    this, SLOT( updateEditActions() ) );

        if ( oldWidget )
        {
            if ( oldWidget->inherits( "QLineEdit" ) )
                disconnect( oldWidget, SIGNAL( textChanged( const QString & ) ),
                            this, SLOT( updateEditActions() ) );
            else if ( oldWidget->inherits( "QMultiLineEdit" ) )
                disconnect( oldWidget, SIGNAL( textChanged() ),
                            this, SLOT( updateEditActions() ) );
        }

        m_connectedToClipboard = false;
    }
}

void KHTMLPartBrowserExtension::setExtensionProxy( KParts::BrowserExtension *proxy )
{
    if ( m_extensionProxy )
        disconnect( m_extensionProxy, SIGNAL( enableAction( const char *, bool ) ),
                    this, SLOT( extensionProxyActionEnabled( const char *, bool ) ) );

    m_extensionProxy = proxy;

    if ( m_extensionProxy )
    {
        connect( m_extensionProxy, SIGNAL( enableAction( const char *, bool ) ),
                 this, SLOT( extensionProxyActionEnabled( const char *, bool ) ) );

        enableAction( "cut", m_extensionProxy->isActionEnabled( "cut" ) );
        enableAction( "copy", m_extensionProxy->isActionEnabled( "copy" ) );
        enableAction( "paste", m_extensionProxy->isActionEnabled( "paste" ) );
    }
    else
    {
        updateEditActions();
        enableAction( "copy", false ); // ### re-check this
    }
}

void KHTMLPartBrowserExtension::cut()
{
    if ( m_extensionProxy )
    {
        callExtensionProxyMethod( "cut()" );
        return;
    }

    ASSERT( m_editableFormWidget );
    if ( !m_editableFormWidget )
        return; // shouldn't happen

    if ( m_editableFormWidget->inherits( "QLineEdit" ) )
        static_cast<QLineEdit *>( &(*m_editableFormWidget) )->cut();
    else if ( m_editableFormWidget->inherits( "QMultiLineEdit" ) )
        static_cast<QMultiLineEdit *>( &(*m_editableFormWidget) )->cut();
}

void KHTMLPartBrowserExtension::copy()
{
    if ( m_extensionProxy )
    {
        callExtensionProxyMethod( "copy()" );
        return;
    }

    kdDebug( 6050 ) << "************! KHTMLPartBrowserExtension::copy()" << endl;
    if ( !m_editableFormWidget )
    {
        // get selected text and paste to the clipboard
        QString text = m_part->selectedText();
        QClipboard *cb = QApplication::clipboard();
        cb->setText(text);
    }
    else
    {
        if ( m_editableFormWidget->inherits( "QLineEdit" ) )
            static_cast<QLineEdit *>( &(*m_editableFormWidget) )->copy();
        else if ( m_editableFormWidget->inherits( "QMultiLineEdit" ) )
            static_cast<QMultiLineEdit *>( &(*m_editableFormWidget) )->copy();
    }
}

void KHTMLPartBrowserExtension::paste()
{
    if ( m_extensionProxy )
    {
        callExtensionProxyMethod( "paste()" );
        return;
    }

    ASSERT( m_editableFormWidget );
    if ( !m_editableFormWidget )
        return; // shouldn't happen

    if ( m_editableFormWidget->inherits( "QLineEdit" ) )
        static_cast<QLineEdit *>( &(*m_editableFormWidget) )->paste();
    else if ( m_editableFormWidget->inherits( "QMultiLineEdit" ) )
        static_cast<QMultiLineEdit *>( &(*m_editableFormWidget) )->paste();
}

void KHTMLPartBrowserExtension::callExtensionProxyMethod( const char *method )
{
    if ( !m_extensionProxy )
        return;

    QMetaData *metaData = m_extensionProxy->metaObject()->slot( method );
    if ( !metaData )
        return;

    KParts::BrowserExtension *ext = static_cast<KParts::BrowserExtension *>( m_extensionProxy );
    (ext->*(metaData->ptr))();
}

void KHTMLPartBrowserExtension::updateEditActions()
{
    if ( !m_editableFormWidget )
    {
        enableAction( "cut", false );
        enableAction( "paste", false );
        return;
    }

    // ### duplicated from KonqMainWindow::slotClipboardDataChanged
    QMimeSource *data = QApplication::clipboard()->data();
    enableAction( "paste", data->provides( "text/plain" ) );

    bool hasSelection = false;

    if ( m_editableFormWidget->inherits( "QLineEdit" ) )
        hasSelection = static_cast<QLineEdit *>( &(*m_editableFormWidget) )->hasMarkedText();
    else if ( m_editableFormWidget->inherits( "khtml::TextAreaWidget" ) )
        hasSelection = static_cast<khtml::TextAreaWidget *>( &(*m_editableFormWidget) )->hasMarkedText();

    enableAction( "copy", hasSelection );
    enableAction( "cut", hasSelection );
}

void KHTMLPartBrowserExtension::extensionProxyActionEnabled( const char *action, bool enable )
{
    // only forward enableAction calls for actions we actually do foward
    if ( strcmp( action, "cut" ) == 0 ||
         strcmp( action, "copy" ) == 0 ||
         strcmp( action, "paste" ) == 0 )
        enableAction( action, enable );
}

void KHTMLPartBrowserExtension::reparseConfiguration()
{
  m_part->reparseConfiguration();
}

void KHTMLPartBrowserExtension::print()
{
  m_part->view()->print();
}

class KHTMLPopupGUIClient::KHTMLPopupGUIClientPrivate
{
public:
  KHTMLPart *m_khtml;
  KURL m_url;
  KURL m_imageURL;
  KAction *m_paPrintFrame;
  KAction *m_paSaveLinkAs;
  KAction *m_paSaveImageAs;
  KAction *m_paCopyLinkLocation;
  KAction *m_paStopAnimations;
  KAction *m_paCopyImageLocation;
  KAction *m_paViewImage;
  KAction *m_paReloadFrame;
  KAction *m_paViewFrameSource;
};


KHTMLPopupGUIClient::KHTMLPopupGUIClient( KHTMLPart *khtml, const QString &doc, const KURL &url )
{
  d = new KHTMLPopupGUIClientPrivate;
  d->m_khtml = khtml;
  d->m_url = url;

  setInstance( khtml->instance() );

  actionCollection()->insert( khtml->actionCollection()->action( "selectAll" ) );
  actionCollection()->insert( khtml->actionCollection()->action( "viewDocumentSource" ) );

  // frameset? -> add "Reload Frame" etc.
  if ( khtml->parentPart() )
  {
    d->m_paReloadFrame = new KAction( i18n( "Reload Frame" ), 0, this, SLOT( slotReloadFrame() ),
                                      actionCollection(), "reloadframe" );
    d->m_paViewFrameSource = new KAction( i18n( "View Frame Source" ), 0, d->m_khtml, SLOT( slotViewDocumentSource() ),
                                          actionCollection(), "viewFrameSource" );
    // This one isn't in khtml_popupmenu.rc anymore, because Print isn't either,
    // and because print frame is already in the toolbar and the menu.
    // But leave this here, so that it's easy to readd it.
    d->m_paPrintFrame = new KAction( i18n( "Print Frame..." ), "fileprint", 0, d->m_khtml->browserExtension(), SLOT( print() ), actionCollection(), "printFrame" );
  }

  actionCollection()->insert( khtml->actionCollection()->action( "setEncoding" ) );

  if ( !url.isEmpty() )
  {
    d->m_paSaveLinkAs = new KAction( i18n( "&Save Link As..." ), 0, this, SLOT( slotSaveLinkAs() ),
                                     actionCollection(), "savelinkas" );
    d->m_paCopyLinkLocation = new KAction( i18n( "Copy Link Location" ), 0, this, SLOT( slotCopyLinkLocation() ),
                                           actionCollection(), "copylinklocation" );
  }

  d->m_paStopAnimations = new KAction( i18n( "Stop Animations" ), 0, this, SLOT( slotStopAnimations() ),
                                       actionCollection(), "stopanimations" );

  DOM::Element e;
  e = khtml->nodeUnderMouse();

  if ( !e.isNull() && (e.elementId() == ID_IMG ||
                       (e.elementId() == ID_INPUT && !static_cast<DOM::HTMLInputElement>(e).src().isEmpty())))
  {
    d->m_imageURL = KURL( d->m_khtml->url(), e.getAttribute( "src" ).string() );
    d->m_paSaveImageAs = new KAction( i18n( "Save Image As..." ), 0, this, SLOT( slotSaveImageAs() ),
                                      actionCollection(), "saveimageas" );
    d->m_paCopyImageLocation = new KAction( i18n( "Copy Image Location" ), 0, this, SLOT( slotCopyImageLocation() ),
                                            actionCollection(), "copyimagelocation" );
    QString name = KStringHandler::csqueeze(d->m_imageURL.fileName()+d->m_imageURL.query(), 25);
    d->m_paViewImage = new KAction( i18n( "View Image (%1)" ).arg(name), 0, this, SLOT( slotViewImage() ),
                                            actionCollection(), "viewimage" );
  }

  setXML( doc );
  setDOMDocument( QDomDocument(), true ); // ### HACK

  QDomElement menu = domDocument().documentElement().namedItem( "Menu" ).toElement();

  if ( actionCollection()->count() > 0 )
    menu.insertBefore( domDocument().createElement( "separator" ), menu.firstChild() );
}

KHTMLPopupGUIClient::~KHTMLPopupGUIClient()
{
  delete d;
}

void KHTMLPopupGUIClient::slotSaveLinkAs()
{
  saveURL( d->m_khtml->widget(), i18n( "Save Link As" ), d->m_url );
}

void KHTMLPopupGUIClient::slotSaveImageAs()
{
  saveURL( d->m_khtml->widget(), i18n( "Save Image As" ), d->m_imageURL );
}

void KHTMLPopupGUIClient::slotCopyLinkLocation()
{
  KURL::List lst;
  lst.append( d->m_url );
  QApplication::clipboard()->setData( KURLDrag::newDrag( lst ) );
}

void KHTMLPopupGUIClient::slotStopAnimations()
{
  d->m_khtml->stopAnimations();
}

void KHTMLPopupGUIClient::slotCopyImageLocation()
{
  KURL::List lst;
  lst.append( d->m_imageURL );
  QApplication::clipboard()->setData( KURLDrag::newDrag( lst ) );
}

void KHTMLPopupGUIClient::slotViewImage()
{
  d->m_khtml->browserExtension()->createNewWindow(d->m_imageURL.url());
}

void KHTMLPopupGUIClient::slotReloadFrame()
{
  KParts::URLArgs args( d->m_khtml->browserExtension()->urlArgs() );
  args.reload = true;
  // reload document
  d->m_khtml->closeURL();
  d->m_khtml->browserExtension()->setURLArgs( args );
  d->m_khtml->openURL( d->m_khtml->url() );
}

void KHTMLPopupGUIClient::saveURL( QWidget *parent, const QString &caption, const KURL &url, const QString &filter, long cacheId, const QString & suggestedFilename )
{
  KFileDialog *dlg = new KFileDialog( QString::null, filter, parent, "filedia", true );

  dlg->setKeepLocation( true );

  dlg->setCaption( caption );

  if (!suggestedFilename.isEmpty())
    dlg->setSelection( suggestedFilename );
  else if (!url.fileName().isEmpty())
    dlg->setSelection( url.fileName() );
  else
    dlg->setSelection( QString::fromLatin1("index.html") );

  if ( dlg->exec() )
  {
    KURL destURL( dlg->selectedURL() );
    if ( !destURL.isMalformed() )
    {
      bool saved = false;
      if (KHTMLPageCache::self()->isValid(cacheId))
      {
        if (destURL.isLocalFile())
        {
          KSaveFile destFile(destURL.path());
          if (destFile.status() == 0)
          {
            KHTMLPageCache::self()->saveData(cacheId, destFile.dataStream());
            saved = true;
          }
        }
        else
        {
          // save to temp file, then move to final destination.
          KTempFile destFile;
          if (destFile.status() == 0)
          {
            KHTMLPageCache::self()->saveData(cacheId, destFile.dataStream());
            destFile.close();
            KURL url2 = KURL();
            url2.setPath(destFile.name());
            KIO::move(url2, destURL);
            saved = true;
          }
        }
      }
      if(!saved)
      {
        /*KIO::Job *job = */ KIO::copy( url, destURL );
        // TODO connect job result, to display errors
      }
    }
  }

  delete dlg;
}

KHTMLPartBrowserHostExtension::KHTMLPartBrowserHostExtension( KHTMLPart *part )
: KParts::BrowserHostExtension( part )
{
  m_part = part;
}

KHTMLPartBrowserHostExtension::~KHTMLPartBrowserHostExtension()
{
}

QStringList KHTMLPartBrowserHostExtension::frameNames() const
{
  return m_part->frameNames();
}

const QList<KParts::ReadOnlyPart> KHTMLPartBrowserHostExtension::frames() const
{
  return m_part->frames();
}

bool KHTMLPartBrowserHostExtension::openURLInFrame( const KURL &url, const KParts::URLArgs &urlArgs )
{
  return m_part->openURLInFrame( url, urlArgs );
}

KHTMLFontSizeAction::KHTMLFontSizeAction( KHTMLPart *part, bool direction, const QString &text, const QString &icon, const QObject *receiver, const char *slot, QObject *parent, const char *name )
    : KAction( text, icon, 0, receiver, slot, parent, name )
{
    m_direction = direction;
    m_part = part;

    m_popup = new QPopupMenu;
    m_popup->insertItem( i18n( "Default font size" ) );

    int m = m_direction ? 1 : -1;

    for ( int i = 1; i < 5; ++i )
    {
        int num = i * m;
        QString numStr = QString::number( num );
        if ( num > 0 ) numStr.prepend( '+' );

        m_popup->insertItem( i18n( "Font Size %1" ).arg( numStr ) );
    }

    connect( m_popup, SIGNAL( activated( int ) ), this, SLOT( slotActivated( int ) ) );
}

KHTMLFontSizeAction::~KHTMLFontSizeAction()
{
    delete m_popup;
}

int KHTMLFontSizeAction::plug( QWidget *w, int index )
{
    int containerId = KAction::plug( w, index );
    if ( containerId == -1 || !w->inherits( "KToolBar" ) )
        return containerId;

    KToolBarButton *button = static_cast<KToolBar *>( w )->getButton( menuId( containerId ) );
    if ( !button )
        return containerId;

    button->setDelayedPopup( m_popup );
    return containerId;
}

void KHTMLFontSizeAction::slotActivated( int id )
{
    int idx = m_popup->indexOf( id );

    if ( idx == 0 )
        m_part->setFontBaseInternal( 0, true );
    else
        m_part->setFontBaseInternal( idx * ( m_direction ? 1 : -1 ), false );
}

using namespace KParts;
#include "khtml_ext.moc"

