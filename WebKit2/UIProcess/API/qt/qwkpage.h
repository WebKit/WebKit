#ifndef qwkpage_h
#define qwkpage_h

#include "qwebkitglobal.h"
#include <QAction>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QUrl>
#include <WebKit2/WKBase.h>
#include <WebKit2/WKPage.h>
#include <WebKit2/WKPageNamespace.h>

class QCursor;
class QWKGraphicsWidget;
class QWKPagePrivate;

class QWEBKIT_EXPORT QWKPage : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title)
    Q_PROPERTY(QUrl url READ url WRITE setUrl)

public:
    enum WebAction {
        NoWebAction = - 1,

        Back,
        Forward,
        Stop,
        Reload,

        WebActionCount
    };

    QWKPage(WKPageNamespaceRef);
    virtual ~QWKPage();

    WKPageRef pageRef() const;

    void load(const QUrl& url);
    void setUrl(const QUrl& url);
    QUrl url() const;

    QString title() const;

    void setViewportSize(const QSize&);

    QAction* action(WebAction action) const;
    void triggerAction(WebAction action, bool checked = false);

    typedef QWKPage* (*CreateNewPageFn)(QWKPage*);
    void setCreateNewPageFunction(CreateNewPageFn function);

public:
    Q_SIGNAL void statusBarMessage(const QString&);
    Q_SIGNAL void titleChanged(const QString&);
    Q_SIGNAL void loadStarted();
    Q_SIGNAL void loadFinished(bool ok);
    Q_SIGNAL void loadProgress(int progress);
    Q_SIGNAL void initialLayoutCompleted();
    Q_SIGNAL void urlChanged(const QUrl&);
    Q_SIGNAL void contentsSizeChanged(const QSize&);
    Q_SIGNAL void cursorChanged(const QCursor&);

protected:
    void timerEvent(QTimerEvent*);

private:
#ifndef QT_NO_ACTION
    Q_PRIVATE_SLOT(d, void _q_webActionTriggered(bool checked));
#endif
    QWKPagePrivate* d;

    friend class QGraphicsWKView;
    friend class QGraphicsWKViewPrivate;
    friend class QWKPagePrivate;
};

#endif /* qwkpage_h */
