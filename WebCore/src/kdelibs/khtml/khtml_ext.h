#ifndef __khtml_ext_h__
#define __khtml_ext_h__

#include "khtml_part.h"

#include <qguardedptr.h>

#include <kaction.h>
#include <kio/global.h>

/**
 * This is the BrowserExtension for a @ref KHTMLPart document. Please see the KParts documentation for
 * more information about the BrowserExtension.
 */
class KHTMLPartBrowserExtension : public KParts::BrowserExtension
{
  Q_OBJECT
  friend class KHTMLPart;
  friend class KHTMLView;
public:
  KHTMLPartBrowserExtension( KHTMLPart *parent, const char *name = 0L );

  virtual int xOffset();
  virtual int yOffset();

  virtual void saveState( QDataStream &stream );
  virtual void restoreState( QDataStream &stream );

    // internal
    void editableWidgetFocused( QWidget *widget );
    void editableWidgetBlurred( QWidget *widget );

    void setExtensionProxy( KParts::BrowserExtension *proxyExtension );

public slots:
    void cut();
    void copy();
    void paste();
    void reparseConfiguration();
    void print();

    // internal . updates the state of the cut/copt/paste action based
    // on whether data is available in the clipboard
    void updateEditActions();

private slots:
    // connected to a frame's browserextensions enableAction signal
    void extensionProxyActionEnabled( const char *action, bool enable );

private:
    void callExtensionProxyMethod( const char *method );

    KHTMLPart *m_part;
    QGuardedPtr<QWidget> m_editableFormWidget;
    QGuardedPtr<KParts::BrowserExtension> m_extensionProxy;
    bool m_connectedToClipboard;
};

class KHTMLPartBrowserHostExtension : public KParts::BrowserHostExtension
{
public:
  KHTMLPartBrowserHostExtension( KHTMLPart *part );
  virtual ~KHTMLPartBrowserHostExtension();

  virtual QStringList frameNames() const;

  virtual const QPtrList<KParts::ReadOnlyPart> frames() const;

  virtual bool openURLInFrame( const KURL &url, const KParts::URLArgs &urlArgs );
private:
  KHTMLPart *m_part;
};

/**
 * @internal
 * INTERNAL class. *NOT* part of the public API.
 */
class KHTMLPopupGUIClient : public QObject, public KXMLGUIClient
{
  Q_OBJECT
public:
  KHTMLPopupGUIClient( KHTMLPart *khtml, const QString &doc, const KURL &url );
  virtual ~KHTMLPopupGUIClient();

  static void saveURL( QWidget *parent, const QString &caption, const KURL &url,
                       const QMap<QString, QString> &metaData = KIO::MetaData(),
                       const QString &filter = QString::null, long cacheId = 0,
                       const QString &suggestedFilename = QString::null );

  static void saveURL( const KURL &url, const KURL &destination,
                       const QMap<QString, QString> &metaData = KIO::MetaData(),
                       long cacheId = 0 );
private slots:
  void slotSaveLinkAs();
  void slotSaveImageAs();
  void slotCopyLinkLocation();
  void slotStopAnimations();
  void slotCopyImageLocation();
  void slotViewImage();
  void slotReloadFrame();
private:
  class KHTMLPopupGUIClientPrivate;
  KHTMLPopupGUIClientPrivate *d;
};

class KHTMLZoomFactorAction : public KAction
{
    Q_OBJECT
public:
    KHTMLZoomFactorAction( KHTMLPart *part, bool direction, const QString &text, const QString &icon, const QObject *receiver, const char *slot, QObject *parent, const char *name );
    virtual ~KHTMLZoomFactorAction();

    virtual int plug( QWidget *w, int index );

private slots:
    void slotActivated( int );
protected slots:
    void slotActivated() { KAction::slotActivated(); }
private:
    QPopupMenu *m_popup;
    bool m_direction;
    KHTMLPart *m_part;
};

#endif
