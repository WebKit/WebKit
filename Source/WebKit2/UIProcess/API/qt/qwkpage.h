#ifndef qwkpage_h
#define qwkpage_h

#include "qwebkitglobal.h"
#include <QAction>
#include <QMenu>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QUrl>
#include <WebKit2/WKBase.h>
#include <WebKit2/WKPage.h>

class QCursor;
class QGraphicsItem;
class QWKContext;
class QWKGraphicsWidget;
class QWKPreferences;
class QWKPagePrivate;
class QtViewportAttributesPrivate;
class QWKHistory;

class QWEBKIT_EXPORT QWKPage : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title)
    Q_PROPERTY(QUrl url READ url WRITE setUrl)

public:
    enum WebAction {
        NoWebAction = - 1,

        OpenLink,
        OpenLinkInNewWindow,
        CopyLinkToClipboard,
        OpenImageInNewWindow,

        Back,
        Forward,
        Stop,
        Reload,

        Cut,
        Copy,
        Paste,
        SelectAll,

        WebActionCount
    };

    class QWEBKIT_EXPORT ViewportAttributes {
    public:
        ViewportAttributes();
        ViewportAttributes(const QWKPage::ViewportAttributes& other);

        ~ViewportAttributes();

        QWKPage::ViewportAttributes& operator=(const QWKPage::ViewportAttributes& other);

        inline qreal initialScaleFactor() const { return m_initialScaleFactor; };
        inline qreal minimumScaleFactor() const { return m_minimumScaleFactor; };
        inline qreal maximumScaleFactor() const { return m_maximumScaleFactor; };
        inline qreal devicePixelRatio() const { return m_devicePixelRatio; };
        inline bool isUserScalable() const { return m_isUserScalable; };
        inline bool isValid() const { return m_isValid; };
        inline QSize size() const { return m_size; };

    private:
        QSharedDataPointer<QtViewportAttributesPrivate> d;
        qreal m_initialScaleFactor;
        qreal m_minimumScaleFactor;
        qreal m_maximumScaleFactor;
        qreal m_devicePixelRatio;
        bool m_isUserScalable;
        bool m_isValid;
        QSize m_size;

        friend class QWKPage;
    };

    QWKPage(QWKContext*);
    virtual ~QWKPage();

    WKPageRef pageRef() const;

    QWKPreferences* preferences() const;

    void load(const QUrl& url);
    void setUrl(const QUrl& url);
    QUrl url() const;

    QString title() const;

    void setViewportSize(const QSize&);
    ViewportAttributes viewportAttributesForSize(const QSize& availableSize) const;

    void setActualVisibleContentsRect(const QRect& rect) const;

    void setResizesToContentsUsingLayoutSize(const QSize& targetLayoutSize);

    QAction* action(WebAction action) const;
    void triggerAction(WebAction action, bool checked = false);

    typedef QWKPage* (*CreateNewPageFn)(QWKPage*);
    void setCreateNewPageFunction(CreateNewPageFn function);

    void setCustomUserAgent(const QString&);
    QString customUserAgent() const;

    qreal textZoomFactor() const;
    qreal pageZoomFactor() const;
    void setTextZoomFactor(qreal zoomFactor);
    void setPageZoomFactor(qreal zoomFactor);
    void setPageAndTextZoomFactors(qreal pageZoomFactor, qreal textZoomFactor);

    QWKHistory* history() const;
    QWKContext* context() const;

    void findZoomableAreaForPoint(const QPoint&);

    bool isConnectedToEngine() const;

public:
    Q_SIGNAL void statusBarMessage(const QString&);
    Q_SIGNAL void toolTipChanged(const QString&);
    Q_SIGNAL void titleChanged(const QString&);
    Q_SIGNAL void loadStarted();
    Q_SIGNAL void loadFinished(bool ok);
    Q_SIGNAL void loadProgress(int progress);
    Q_SIGNAL void initialLayoutCompleted();
    Q_SIGNAL void urlChanged(const QUrl&);
    Q_SIGNAL void contentsSizeChanged(const QSize&);
    Q_SIGNAL void scrollRequested(int dx, int dy);
    Q_SIGNAL void cursorChanged(const QCursor&);
    Q_SIGNAL void viewportChangeRequested();
    Q_SIGNAL void windowCloseRequested();
    Q_SIGNAL void zoomableAreaFound(const QRect&);
    Q_SIGNAL void focusNextPrevChild(bool);
    Q_SIGNAL void showContextMenu(QSharedPointer<QMenu>);
    Q_SIGNAL void engineConnectionChanged(bool connected);

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
