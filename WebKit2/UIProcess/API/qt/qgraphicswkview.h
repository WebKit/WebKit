#ifndef qgraphicswkview_h
#define qgraphicswkview_h

#include "qwebkitglobal.h"

#include <WebKit2/WKBase.h>
#include <QGraphicsWidget>
#include "qwkpage.h"

QT_BEGIN_NAMESPACE
class QCursor;
QT_END_NAMESPACE

class QGraphicsWKViewPrivate;

WKStringRef WKStringCreateWithQString(QString qString);
QString WKStringCopyQString(WKStringRef stringRef);

class QWEBKIT_EXPORT QGraphicsWKView : public QGraphicsWidget {
    Q_OBJECT
    Q_PROPERTY(QString title READ title)
    Q_PROPERTY(QUrl url READ url WRITE setUrl)

public:
    enum BackingStoreType { Simple, Tiled };
    QGraphicsWKView(WKPageNamespaceRef namespaceRef, BackingStoreType backingStoreType = Simple, QGraphicsItem* parent = 0);

    virtual ~QGraphicsWKView();

    QWKPage* page() const;

    virtual void setGeometry(const QRectF&);

    void load(const QUrl&);
    void setUrl(const QUrl&);
    QUrl url() const;

    QString title() const;

    void triggerPageAction(QWKPage::WebAction action, bool checked = false);

    virtual void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*);
    virtual QVariant itemChange(GraphicsItemChange, const QVariant&);
    virtual bool event(QEvent*);
    virtual QSizeF sizeHint(Qt::SizeHint, const QSizeF&) const;
    virtual QVariant inputMethodQuery(Qt::InputMethodQuery) const;

    // FIXME: should not be public
    virtual QRectF visibleRect() const;

public:
    Q_SIGNAL void titleChanged(const QString& title);
    Q_SIGNAL void loadStarted();
    Q_SIGNAL void loadFinished(bool ok);
    Q_SIGNAL void loadProgress(int progress);
    Q_SIGNAL void initialLayoutCompleted();
    Q_SIGNAL void urlChanged(const QUrl&);

public Q_SLOTS:
    void back();
    void forward();
    void reload();
    void stop();

protected:
    virtual void keyPressEvent(QKeyEvent*);
    virtual void keyReleaseEvent(QKeyEvent*);
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent*);
    virtual void mousePressEvent(QGraphicsSceneMouseEvent*);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent*);
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*);
    virtual void wheelEvent(QGraphicsSceneWheelEvent*);
    virtual void touchEvent(QTouchEvent*);

    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent*);

    Q_SLOT void updateCursor(const QCursor&);

private:
    QGraphicsWKViewPrivate* d;
    friend class QGraphicsWKViewPrivate;
};

#endif /* qgraphicswkview_h */
