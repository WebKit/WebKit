/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2009 Girish Ramakrishnan <girish@forwardbias.in>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "qgraphicswebview.h"

#include "qwebframe.h"
#include "qwebframe_p.h"
#include "qwebpage.h"
#include "qwebpage_p.h"
#include "QWebPageClient.h"
#include <FrameView.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qsharedpointer.h>
#include <QtCore/qtimer.h>
#include <QtGui/qapplication.h>
#include <QtGui/qgraphicsscene.h>
#include <QtGui/qgraphicssceneevent.h>
#include <QtGui/qgraphicsview.h>
#include <QtGui/qpixmapcache.h>
#include <QtGui/qstyleoption.h>
#if defined(Q_WS_X11)
#include <QX11Info>
#endif
#include <Settings.h>

#if USE(ACCELERATED_COMPOSITING)

// the overlay is here for one reason only: to have the scroll-bars and other
// extra UI elements appear on top of any QGraphicsItems created by CSS compositing layers
class QGraphicsWebViewOverlay : public QGraphicsItem {
    public:
    QGraphicsWebViewOverlay(QGraphicsWebView* view)
            :QGraphicsItem(view)
            , q(view)
    {
        setPos(0, 0);
        setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
        setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    }

    QRectF boundingRect() const
    {
        return q->boundingRect();
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* options, QWidget*)
    {
        q->page()->mainFrame()->render(painter, static_cast<QWebFrame::RenderLayer>(QWebFrame::AllLayers&(~QWebFrame::ContentsLayer)), options->exposedRect.toRect());
    }

    friend class QGraphicsWebView;
    QGraphicsWebView* q;
};

#endif

class QGraphicsWebViewPrivate : public QWebPageClient {
public:
    QGraphicsWebViewPrivate(QGraphicsWebView* parent)
        : q(parent)
        , page(0)
#if USE(ACCELERATED_COMPOSITING)
        , rootGraphicsLayer(0)
        , shouldSync(false)
#endif
    {
#if USE(ACCELERATED_COMPOSITING)
        // the overlay and stays alive for the lifetime of
        // this QGraphicsWebView as the scrollbars are needed when there's no compositing
        q->setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);
        syncMetaMethod = q->metaObject()->method(q->metaObject()->indexOfMethod("syncLayers()"));
#endif
    }

    virtual ~QGraphicsWebViewPrivate();
    virtual void scroll(int dx, int dy, const QRect&);
    virtual void update(const QRect& dirtyRect);
    virtual void setInputMethodEnabled(bool enable);
    virtual bool inputMethodEnabled() const;
#if QT_VERSION >= 0x040600
    virtual void setInputMethodHint(Qt::InputMethodHint hint, bool enable);
#endif

#ifndef QT_NO_CURSOR
    virtual QCursor cursor() const;
    virtual void updateCursor(const QCursor& cursor);
#endif

    virtual QPalette palette() const;
    virtual int screenNumber() const;
    virtual QWidget* ownerWidget() const;

    virtual QObject* pluginParent() const;

    virtual QStyle* style() const;

#if USE(ACCELERATED_COMPOSITING)
    virtual void setRootGraphicsLayer(QGraphicsItem* layer);
    virtual void markForSync(bool scheduleSync);
    void updateCompositingScrollPosition();
#endif

    void syncLayers();
    void _q_doLoadFinished(bool success);

    QGraphicsWebView* q;
    QWebPage* page;
#if USE(ACCELERATED_COMPOSITING)
    QGraphicsItem* rootGraphicsLayer;

    // the overlay gets instantiated when the root layer is attached, and get deleted when it's detached
    QSharedPointer<QGraphicsWebViewOverlay> overlay;

    // we need to sync the layers if we get a special call from the WebCore
    // compositor telling us to do so. We'll get that call from ChromeClientQt
    bool shouldSync;

    // we have to flush quite often, so we use a meta-method instead of QTimer::singleShot for putting the event in the queue
    QMetaMethod syncMetaMethod;

    // we need to put the root graphics layer behind the overlay (which contains the scrollbar)
    enum { RootGraphicsLayerZValue, OverlayZValue };
#endif
};

QGraphicsWebViewPrivate::~QGraphicsWebViewPrivate()
{
#if USE(ACCELERATED_COMPOSITING)
    if (rootGraphicsLayer) {
        // we don't need to delete the root graphics layer
        // The lifecycle is managed in GraphicsLayerQt.cpp
        rootGraphicsLayer->setParentItem(0);
        q->scene()->removeItem(rootGraphicsLayer);
    }
#endif
}

#if USE(ACCELERATED_COMPOSITING)
void QGraphicsWebViewPrivate::setRootGraphicsLayer(QGraphicsItem* layer)
{
    if (rootGraphicsLayer) {
        rootGraphicsLayer->setParentItem(0);
        q->scene()->removeItem(rootGraphicsLayer);
        QWebFramePrivate::core(q->page()->mainFrame())->view()->syncCompositingStateRecursive();
    }

    rootGraphicsLayer = layer;

    if (layer) {
        layer->setFlag(QGraphicsItem::ItemClipsChildrenToShape, true);
        layer->setParentItem(q);
        layer->setZValue(RootGraphicsLayerZValue);
        if (!overlay) {
            overlay = QSharedPointer<QGraphicsWebViewOverlay>(new QGraphicsWebViewOverlay(q));
            overlay->setZValue(OverlayZValue);
        }
        updateCompositingScrollPosition();
    } else {
        // we don't have compositing layers, we can render the scrollbars and content in one go
        overlay.clear();
    }
}

void QGraphicsWebViewPrivate::markForSync(bool scheduleSync)
{
    shouldSync = true;
    if (scheduleSync)
        syncMetaMethod.invoke(q, Qt::QueuedConnection);
}

void QGraphicsWebViewPrivate::updateCompositingScrollPosition()
{
    if (rootGraphicsLayer && q->page() && q->page()->mainFrame()) {
        const QPoint scrollPosition = q->page()->mainFrame()->scrollPosition();
        rootGraphicsLayer->setPos(-scrollPosition);
    }
}

#endif

void QGraphicsWebViewPrivate::syncLayers()
{
#if USE(ACCELERATED_COMPOSITING)
    if (shouldSync) {
        QWebFramePrivate::core(q->page()->mainFrame())->view()->syncCompositingStateRecursive();
        shouldSync = false;
    }
#endif
}

void QGraphicsWebViewPrivate::_q_doLoadFinished(bool success)
{
    // If the page had no title, still make sure it gets the signal
    if (q->title().isEmpty())
        emit q->urlChanged(q->url());

    emit q->loadFinished(success);
}

void QGraphicsWebViewPrivate::scroll(int dx, int dy, const QRect& rectToScroll)
{
    q->scroll(qreal(dx), qreal(dy), QRectF(rectToScroll));
#if USE(ACCELERATED_COMPOSITING)
    updateCompositingScrollPosition();
#endif
}

void QGraphicsWebViewPrivate::update(const QRect & dirtyRect)
{
    q->update(QRectF(dirtyRect));
#if USE(ACCELERATED_COMPOSITING)
    if (overlay)
        overlay->update(QRectF(dirtyRect));
    syncLayers();
#endif
}


void QGraphicsWebViewPrivate::setInputMethodEnabled(bool enable)
{
#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    q->setFlag(QGraphicsItem::ItemAcceptsInputMethod, enable);
#endif
}

bool QGraphicsWebViewPrivate::inputMethodEnabled() const
{
#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    return q->flags() & QGraphicsItem::ItemAcceptsInputMethod;
#else
    return false;
#endif
}

#if QT_VERSION >= 0x040600
void QGraphicsWebViewPrivate::setInputMethodHint(Qt::InputMethodHint hint, bool enable)
{
    if (enable)
        q->setInputMethodHints(q->inputMethodHints() | hint);
    else
        q->setInputMethodHints(q->inputMethodHints() & ~hint);
}
#endif
#ifndef QT_NO_CURSOR
QCursor QGraphicsWebViewPrivate::cursor() const
{
    return q->cursor();
}

void QGraphicsWebViewPrivate::updateCursor(const QCursor& cursor)
{
    q->setCursor(cursor);
}
#endif

QPalette QGraphicsWebViewPrivate::palette() const
{
    return q->palette();
}

int QGraphicsWebViewPrivate::screenNumber() const
{
#if defined(Q_WS_X11)
    const QList<QGraphicsView*> views = q->scene()->views();

    if (!views.isEmpty())
        return views.at(0)->x11Info().screen();
#endif

    return 0;
}

QWidget* QGraphicsWebViewPrivate::ownerWidget() const
{
    const QList<QGraphicsView*> views = q->scene()->views();
    return views.value(0);
}

QObject* QGraphicsWebViewPrivate::pluginParent() const
{
    return q;
}

QStyle* QGraphicsWebViewPrivate::style() const
{
    return q->style();
}

/*!
    \class QGraphicsWebView
    \brief The QGraphicsWebView class allows Web content to be added to a GraphicsView.
    \since 4.6

    An instance of this class renders Web content from a URL or supplied as data, using
    features of the QtWebKit module.

    If the width and height of the item are not set, they will default to 800 and 600,
    respectively. If the Web page contents is larger than that, scrollbars will be shown
    if not disabled explicitly.

    \section1 Browser Features

    Many of the functions, signals and properties provided by QWebView are also available
    for this item, making it simple to adapt existing code to use QGraphicsWebView instead
    of QWebView.

    The item uses a QWebPage object to perform the rendering of Web content, and this can
    be obtained with the page() function, enabling the document itself to be accessed and
    modified.

    As with QWebView, the item records the browsing history using a QWebHistory object,
    accessible using the history() function. The QWebSettings object that defines the
    configuration of the browser can be obtained with the settings() function, enabling
    features like plugin support to be customized for each item.

    \sa QWebView, QGraphicsTextItem
*/

/*!
    \fn void QGraphicsWebView::titleChanged(const QString &title)

    This signal is emitted whenever the \a title of the main frame changes.

    \sa title()
*/

/*!
    \fn void QGraphicsWebView::urlChanged(const QUrl &url)

    This signal is emitted when the \a url of the view changes.

    \sa url(), load()
*/

/*!
    \fn void QGraphicsWebView::iconChanged()

    This signal is emitted whenever the icon of the page is loaded or changes.

    In order for icons to be loaded, you will need to set an icon database path
    using QWebSettings::setIconDatabasePath().

    \sa icon(), QWebSettings::setIconDatabasePath()
*/

/*!
    \fn void QGraphicsWebView::loadStarted()

    This signal is emitted when a new load of the page is started.

    \sa loadProgress(), loadFinished()
*/

/*!
    \fn void QGraphicsWebView::loadFinished(bool ok)

    This signal is emitted when a load of the page is finished.
    \a ok will indicate whether the load was successful or any error occurred.

    \sa loadStarted()
*/

/*!
    Constructs an empty QGraphicsWebView with parent \a parent.

    \sa load()
*/
QGraphicsWebView::QGraphicsWebView(QGraphicsItem* parent)
    : QGraphicsWidget(parent)
    , d(new QGraphicsWebViewPrivate(this))
{
#if QT_VERSION >= 0x040600
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
#endif
    setAcceptDrops(true);
    setAcceptHoverEvents(true);
#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    setAcceptTouchEvents(true);
#endif
    setFocusPolicy(Qt::StrongFocus);
    setFlag(QGraphicsItem::ItemClipsChildrenToShape, true);
}

/*!
    Destroys the item.
*/
QGraphicsWebView::~QGraphicsWebView()
{
    if (d->page) {
#if QT_VERSION >= 0x040600
        d->page->d->view.clear();
#else
        d->page->d->view = 0;
#endif
        d->page->d->client = 0; // unset the page client
    }

    if (d->page && d->page->parent() == this)
        delete d->page;

    delete d;
}

/*!
    Returns a pointer to the underlying web page.

    \sa setPage()
*/
QWebPage* QGraphicsWebView::page() const
{
    if (!d->page) {
        QGraphicsWebView* that = const_cast<QGraphicsWebView*>(this);
        QWebPage* page = new QWebPage(that);

        // Default to not having a background, in the case
        // the page doesn't provide one.
        QPalette palette = QApplication::palette();
        palette.setBrush(QPalette::Base, QColor::fromRgbF(0, 0, 0, 0));
        page->setPalette(palette);

        that->setPage(page);
    }

    return d->page;
}

/*! \reimp
*/
void QGraphicsWebView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
#if USE(ACCELERATED_COMPOSITING)
    page()->mainFrame()->render(painter, d->overlay ? QWebFrame::ContentsLayer : QWebFrame::AllLayers, option->exposedRect.toAlignedRect());
#else
    page()->mainFrame()->render(painter, QWebFrame::AllLayers, option->exposedRect.toRect());
#endif
}

/*! \reimp
*/
bool QGraphicsWebView::sceneEvent(QEvent* event)
{
    // Re-implemented in order to allows fixing event-related bugs in patch releases.

#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    if (d->page && (event->type() == QEvent::TouchBegin
                || event->type() == QEvent::TouchEnd
                || event->type() == QEvent::TouchUpdate)) {
        d->page->event(event);
        if (event->isAccepted())
            return true;
    }
#endif

    return QGraphicsWidget::sceneEvent(event);
}

/*! \reimp
*/
QVariant QGraphicsWebView::itemChange(GraphicsItemChange change, const QVariant& value)
{
    switch (change) {
    // Differently from QWebView, it is interesting to QGraphicsWebView to handle
    // post mouse cursor change notifications. Reason: 'ItemCursorChange' is sent
    // as the first action in QGraphicsItem::setCursor implementation, and at that
    // item widget's cursor has not been effectively changed yet.
    // After cursor is properly set (at 'ItemCursorHasChanged' emission time), we
    // fire 'CursorChange'.
    case ItemCursorChange:
        return value;
    case ItemCursorHasChanged:
        QEvent event(QEvent::CursorChange);
        QApplication::sendEvent(this, &event);
        return value;
    }

    return QGraphicsWidget::itemChange(change, value);
}

/*! \reimp
*/
QSizeF QGraphicsWebView::sizeHint(Qt::SizeHint which, const QSizeF& constraint) const
{
    if (which == Qt::PreferredSize)
        return QSizeF(800, 600); // ###
    return QGraphicsWidget::sizeHint(which, constraint);
}

/*! \reimp
*/
QVariant QGraphicsWebView::inputMethodQuery(Qt::InputMethodQuery query) const
{
    if (d->page)
        return d->page->inputMethodQuery(query);
    return QVariant();
}

/*! \reimp
*/
bool QGraphicsWebView::event(QEvent* event)
{
    // Re-implemented in order to allows fixing event-related bugs in patch releases.

    if (d->page) {
#ifndef QT_NO_CONTEXTMENU
        if (event->type() == QEvent::GraphicsSceneContextMenu) {
            if (!isEnabled())
                return false;

            QGraphicsSceneContextMenuEvent* ev = static_cast<QGraphicsSceneContextMenuEvent*>(event);
            QContextMenuEvent fakeEvent(QContextMenuEvent::Reason(ev->reason()), ev->pos().toPoint());
            if (d->page->swallowContextMenuEvent(&fakeEvent)) {
                event->accept();
                return true;
            }
            d->page->updatePositionDependentActions(fakeEvent.pos());
        } else
#endif // QT_NO_CONTEXTMENU
        {
#ifndef QT_NO_CURSOR
            if (event->type() == QEvent::CursorChange) {
                // An unsetCursor will set the cursor to Qt::ArrowCursor.
                // Thus this cursor change might be a QWidget::unsetCursor()
                // If this is not the case and it came from WebCore, the
                // QWebPageClient already has set its cursor internally
                // to Qt::ArrowCursor, so updating the cursor is always
                // right, as it falls back to the last cursor set by
                // WebCore.
                // FIXME: Add a QEvent::CursorUnset or similar to Qt.
                if (cursor().shape() == Qt::ArrowCursor)
                    d->resetCursor();
            }
#endif
        }
    }
    return QGraphicsWidget::event(event);
}

/*!
    Makes \a page the new web page of the web graphicsitem.

    The parent QObject of the provided page remains the owner
    of the object. If the current document is a child of the web
    view, it will be deleted.

    \sa page()
*/
void QGraphicsWebView::setPage(QWebPage* page)
{
    if (d->page == page)
        return;

    if (d->page) {
        d->page->d->client = 0; // unset the page client
        if (d->page->parent() == this)
            delete d->page;
        else
            d->page->disconnect(this);
    }

    d->page = page;
    if (!d->page)
        return;
#if USE(ACCELERATED_COMPOSITING)
    if (d->overlay)
        d->overlay->prepareGeometryChange();
#endif
    d->page->d->client = d; // set the page client

    QSize size = geometry().size().toSize();
    page->setViewportSize(size);

    QWebFrame* mainFrame = d->page->mainFrame();

    connect(mainFrame, SIGNAL(titleChanged(QString)),
            this, SIGNAL(titleChanged(QString)));
    connect(mainFrame, SIGNAL(iconChanged()),
            this, SIGNAL(iconChanged()));
    connect(mainFrame, SIGNAL(urlChanged(QUrl)),
            this, SIGNAL(urlChanged(QUrl)));
    connect(d->page, SIGNAL(loadStarted()),
            this, SIGNAL(loadStarted()));
    connect(d->page, SIGNAL(loadProgress(int)),
            this, SIGNAL(loadProgress(int)));
    connect(d->page, SIGNAL(loadFinished(bool)),
            this, SLOT(_q_doLoadFinished(bool)));
    connect(d->page, SIGNAL(statusBarMessage(QString)),
            this, SIGNAL(statusBarMessage(QString)));
    connect(d->page, SIGNAL(linkClicked(QUrl)),
            this, SIGNAL(linkClicked(QUrl)));
}

/*!
    \property QGraphicsWebView::url
    \brief the url of the web page currently viewed

    Setting this property clears the view and loads the URL.

    By default, this property contains an empty, invalid URL.

    \sa load(), urlChanged()
*/

void QGraphicsWebView::setUrl(const QUrl &url)
{
    page()->mainFrame()->setUrl(url);
}

QUrl QGraphicsWebView::url() const
{
    if (d->page)
        return d->page->mainFrame()->url();

    return QUrl();
}

/*!
    \property QGraphicsWebView::title
    \brief the title of the web page currently viewed

    By default, this property contains an empty string.

    \sa titleChanged()
*/
QString QGraphicsWebView::title() const
{
    if (d->page)
        return d->page->mainFrame()->title();

    return QString();
}

/*!
    \property QGraphicsWebView::icon
    \brief the icon associated with the web page currently viewed

    By default, this property contains a null icon.

    \sa iconChanged(), QWebSettings::iconForUrl()
*/
QIcon QGraphicsWebView::icon() const
{
    if (d->page)
        return d->page->mainFrame()->icon();

    return QIcon();
}

/*!
    \property QGraphicsWebView::zoomFactor
    \brief the zoom factor for the view
*/

void QGraphicsWebView::setZoomFactor(qreal factor)
{
    if (factor == page()->mainFrame()->zoomFactor())
        return;

    page()->mainFrame()->setZoomFactor(factor);
}

qreal QGraphicsWebView::zoomFactor() const
{
    return page()->mainFrame()->zoomFactor();
}

/*! \reimp
*/
void QGraphicsWebView::updateGeometry()
{

#if USE(ACCELERATED_COMPOSITING)
    if (d->overlay)
        d->overlay->prepareGeometryChange();
#endif

    QGraphicsWidget::updateGeometry();

    if (!d->page)
        return;

    QSize size = geometry().size().toSize();
    d->page->setViewportSize(size);
}

/*! \reimp
*/
void QGraphicsWebView::setGeometry(const QRectF& rect)
{
    QGraphicsWidget::setGeometry(rect);

#if USE(ACCELERATED_COMPOSITING)
    if (d->overlay)
        d->overlay->prepareGeometryChange();
#endif

    if (!d->page)
        return;

    // NOTE: call geometry() as setGeometry ensures that
    // the geometry is within legal bounds (minimumSize, maximumSize)
    QSize size = geometry().size().toSize();
    d->page->setViewportSize(size);
}

/*!
    Convenience slot that stops loading the document.

    \sa reload(), loadFinished()
*/
void QGraphicsWebView::stop()
{
    if (d->page)
        d->page->triggerAction(QWebPage::Stop);
}

/*!
    Convenience slot that loads the previous document in the list of documents
    built by navigating links. Does nothing if there is no previous document.

    \sa forward()
*/
void QGraphicsWebView::back()
{
    if (d->page)
        d->page->triggerAction(QWebPage::Back);
}

/*!
    Convenience slot that loads the next document in the list of documents
    built by navigating links. Does nothing if there is no next document.

    \sa back()
*/
void QGraphicsWebView::forward()
{
    if (d->page)
        d->page->triggerAction(QWebPage::Forward);
}

/*!
    Reloads the current document.

    \sa stop(), loadStarted()
*/
void QGraphicsWebView::reload()
{
    if (d->page)
        d->page->triggerAction(QWebPage::Reload);
}

/*!
    Loads the specified \a url and displays it.

    \note The view remains the same until enough data has arrived to display the new \a url.

    \sa setUrl(), url(), urlChanged()
*/
void QGraphicsWebView::load(const QUrl& url)
{
    page()->mainFrame()->load(url);
}

/*!
    \fn void QGraphicsWebView::load(const QNetworkRequest &request, QNetworkAccessManager::Operation operation, const QByteArray &body)

    Loads a network request, \a request, using the method specified in \a operation.

    \a body is optional and is only used for POST operations.

    \note The view remains the same until enough data has arrived to display the new url.

    \sa url(), urlChanged()
*/

void QGraphicsWebView::load(const QNetworkRequest& request,
                    QNetworkAccessManager::Operation operation,
                    const QByteArray& body)
{
    page()->mainFrame()->load(request, operation, body);
}

/*!
    Sets the content of the web view to the specified \a html.

    External objects such as stylesheets or images referenced in the HTML
    document are located relative to \a baseUrl.

    The \a html is loaded immediately; external objects are loaded asynchronously.

    When using this method, WebKit assumes that external resources such as
    JavaScript programs or style sheets are encoded in UTF-8 unless otherwise
    specified. For example, the encoding of an external script can be specified
    through the charset attribute of the HTML script tag. Alternatively, the
    encoding can also be specified by the web server.

    \sa load(), setContent(), QWebFrame::toHtml()
*/
void QGraphicsWebView::setHtml(const QString& html, const QUrl& baseUrl)
{
    page()->mainFrame()->setHtml(html, baseUrl);
}

/*!
    Sets the content of the web graphicsitem to the specified content \a data. If the \a mimeType argument
    is empty it is currently assumed that the content is HTML but in future versions we may introduce
    auto-detection.

    External objects referenced in the content are located relative to \a baseUrl.

    The \a data is loaded immediately; external objects are loaded asynchronously.

    \sa load(), setHtml(), QWebFrame::toHtml()
*/
void QGraphicsWebView::setContent(const QByteArray& data, const QString& mimeType, const QUrl& baseUrl)
{
    page()->mainFrame()->setContent(data, mimeType, baseUrl);
}

/*!
    Returns a pointer to the view's history of navigated web pages.

    It is equivalent to

    \snippet webkitsnippets/qtwebkit_qwebview_snippet.cpp 0
*/
QWebHistory* QGraphicsWebView::history() const
{
    return page()->history();
}

/*!
    \property QGraphicsWebView::modified
    \brief whether the document was modified by the user

    Parts of HTML documents can be editable for example through the
    \c{contenteditable} attribute on HTML elements.

    By default, this property is false.
*/
bool QGraphicsWebView::isModified() const
{
    if (d->page)
        return d->page->isModified();
    return false;
}

/*!
    Returns a pointer to the view/page specific settings object.

    It is equivalent to

    \snippet webkitsnippets/qtwebkit_qwebview_snippet.cpp 1

    \sa QWebSettings::globalSettings()
*/
QWebSettings* QGraphicsWebView::settings() const
{
    return page()->settings();
}

/*!
    Returns a pointer to a QAction that encapsulates the specified web action \a action.
*/
QAction *QGraphicsWebView::pageAction(QWebPage::WebAction action) const
{
    return page()->action(action);
}

/*!
    Triggers the specified \a action. If it is a checkable action the specified
    \a checked state is assumed.

    \sa pageAction()
*/
void QGraphicsWebView::triggerPageAction(QWebPage::WebAction action, bool checked)
{
    page()->triggerAction(action, checked);
}

/*!
    Finds the specified string, \a subString, in the page, using the given \a options.

    If the HighlightAllOccurrences flag is passed, the function will highlight all occurrences
    that exist in the page. All subsequent calls will extend the highlight, rather than
    replace it, with occurrences of the new string.

    If the HighlightAllOccurrences flag is not passed, the function will select an occurrence
    and all subsequent calls will replace the current occurrence with the next one.

    To clear the selection, just pass an empty string.

    Returns true if \a subString was found; otherwise returns false.

    \sa QWebPage::selectedText(), QWebPage::selectionChanged()
*/
bool QGraphicsWebView::findText(const QString &subString, QWebPage::FindFlags options)
{
    if (d->page)
        return d->page->findText(subString, options);
    return false;
}

/*! \reimp
*/
void QGraphicsWebView::hoverMoveEvent(QGraphicsSceneHoverEvent* ev)
{
    if (d->page) {
        const bool accepted = ev->isAccepted();
        QMouseEvent me = QMouseEvent(QEvent::MouseMove,
                ev->pos().toPoint(), Qt::NoButton,
                Qt::NoButton, Qt::NoModifier);
        d->page->event(&me);
        ev->setAccepted(accepted);
    }

    if (!ev->isAccepted())
        QGraphicsItem::hoverMoveEvent(ev);
}

/*! \reimp
*/
void QGraphicsWebView::hoverLeaveEvent(QGraphicsSceneHoverEvent* ev)
{
    Q_UNUSED(ev);
}

/*! \reimp
*/
void QGraphicsWebView::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
    if (d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }

    if (!ev->isAccepted())
        QGraphicsItem::mouseMoveEvent(ev);
}

/*! \reimp
*/
void QGraphicsWebView::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
    if (d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }

    if (!ev->isAccepted())
        QGraphicsItem::mousePressEvent(ev);
}

/*! \reimp
*/
void QGraphicsWebView::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
    if (d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }

    if (!ev->isAccepted())
        QGraphicsItem::mouseReleaseEvent(ev);
}

/*! \reimp
*/
void QGraphicsWebView::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* ev)
{
    if (d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }

    if (!ev->isAccepted())
        QGraphicsItem::mouseDoubleClickEvent(ev);
}

/*! \reimp
*/
void QGraphicsWebView::keyPressEvent(QKeyEvent* ev)
{
    if (d->page)
        d->page->event(ev);

    if (!ev->isAccepted())
        QGraphicsItem::keyPressEvent(ev);
}

/*! \reimp
*/
void QGraphicsWebView::keyReleaseEvent(QKeyEvent* ev)
{
    if (d->page)
        d->page->event(ev);

    if (!ev->isAccepted())
        QGraphicsItem::keyReleaseEvent(ev);
}

/*! \reimp
*/
void QGraphicsWebView::focusInEvent(QFocusEvent* ev)
{
    if (d->page)
        d->page->event(ev);
    else
        QGraphicsItem::focusInEvent(ev);
}

/*! \reimp
*/
void QGraphicsWebView::focusOutEvent(QFocusEvent* ev)
{
    if (d->page)
        d->page->event(ev);
    else
        QGraphicsItem::focusOutEvent(ev);
}

/*! \reimp
*/
bool QGraphicsWebView::focusNextPrevChild(bool next)
{
    if (d->page)
        return d->page->focusNextPrevChild(next);

    return QGraphicsWidget::focusNextPrevChild(next);
}

/*! \reimp
*/
void QGraphicsWebView::dragEnterEvent(QGraphicsSceneDragDropEvent* ev)
{
#ifndef QT_NO_DRAGANDDROP
    if (d->page)
        d->page->event(ev);
#else
    Q_UNUSED(ev);
#endif
}

/*! \reimp
*/
void QGraphicsWebView::dragLeaveEvent(QGraphicsSceneDragDropEvent* ev)
{
#ifndef QT_NO_DRAGANDDROP
    if (d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }

    if (!ev->isAccepted())
        QGraphicsWidget::dragLeaveEvent(ev);
#else
    Q_UNUSED(ev);
#endif
}

/*! \reimp
*/
void QGraphicsWebView::dragMoveEvent(QGraphicsSceneDragDropEvent* ev)
{
#ifndef QT_NO_DRAGANDDROP
    if (d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }

    if (!ev->isAccepted())
        QGraphicsWidget::dragMoveEvent(ev);
#else
    Q_UNUSED(ev);
#endif
}

/*! \reimp
*/
void QGraphicsWebView::dropEvent(QGraphicsSceneDragDropEvent* ev)
{
#ifndef QT_NO_DRAGANDDROP
    if (d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }

    if (!ev->isAccepted())
        QGraphicsWidget::dropEvent(ev);
#else
    Q_UNUSED(ev);
#endif
}

#ifndef QT_NO_CONTEXTMENU
/*! \reimp
*/
void QGraphicsWebView::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
    if (d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }
}
#endif // QT_NO_CONTEXTMENU

#ifndef QT_NO_WHEELEVENT
/*! \reimp
*/
void QGraphicsWebView::wheelEvent(QGraphicsSceneWheelEvent* ev)
{
    if (d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }

    if (!ev->isAccepted())
        QGraphicsItem::wheelEvent(ev);
}
#endif // QT_NO_WHEELEVENT

/*! \reimp
*/
void QGraphicsWebView::inputMethodEvent(QInputMethodEvent* ev)
{
    if (d->page)
        d->page->event(ev);

    if (!ev->isAccepted())
        QGraphicsItem::inputMethodEvent(ev);
}

/*!
    \fn void QGraphicsWebView::statusBarMessage(const QString& text)

    This signal is emitted when the statusbar \a text is changed by the page.
*/

/*!
    \fn void QGraphicsWebView::loadProgress(int progress)

    This signal is emitted every time an element in the web page
    completes loading and the overall loading progress advances.

    This signal tracks the progress of all child frames.

    The current value is provided by \a progress and scales from 0 to 100,
    which is the default range of QProgressBar.

    \sa loadStarted(), loadFinished()
*/

/*!
    \fn void QGraphicsWebView::linkClicked(const QUrl &url)

    This signal is emitted whenever the user clicks on a link and the page's linkDelegationPolicy
    property is set to delegate the link handling for the specified \a url.

    \sa QWebPage::linkDelegationPolicy()
*/

#include "moc_qgraphicswebview.cpp"
