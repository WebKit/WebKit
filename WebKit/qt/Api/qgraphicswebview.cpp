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
#include "qwebpage.h"
#include "qwebpage_p.h"
#include "QWebPageClient.h"
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsView>
#include <QtGui/qapplication.h>
#include <QtGui/qgraphicssceneevent.h>
#include <QtGui/qstyleoption.h>
#if defined(Q_WS_X11)
#include <QX11Info>
#endif

class QGraphicsWebViewPrivate : public QWebPageClient {
public:
    QGraphicsWebViewPrivate(QGraphicsWebView* parent)
        : q(parent)
        , page(0)
    {}

    virtual void scroll(int dx, int dy, const QRect&);
    virtual void update(const QRect& dirtyRect);
    virtual void setInputMethodEnabled(bool enable);
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

    void _q_doLoadFinished(bool success);

    QGraphicsWebView* q;
    QWebPage* page;
};

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
}

void QGraphicsWebViewPrivate::update(const QRect & dirtyRect)
{
    q->update(QRectF(dirtyRect));
}


void QGraphicsWebViewPrivate::setInputMethodEnabled(bool enable)
{
#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    q->setFlag(QGraphicsItem::ItemAcceptsInputMethod, enable);
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

/*!
    \class QGraphicsWebView
    \brief The QGraphicsWebView class allows web content to be added to a GraphicsView.
    \since 4.6

    A WebGraphicsItem renders web content based on a URL or set data.

    If the width and height of the item is not set, they will
    dynamically adjust to a size appropriate for the content.
    This width may be large (eg. 980) for typical online web pages.
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
    setAcceptHoverEvents(true);
    setFocusPolicy(Qt::StrongFocus);
}

/*!
    Destroys the web graphicsitem.
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
    page()->mainFrame()->render(painter, option->exposedRect.toRect());
}

/*! \reimp
*/
bool QGraphicsWebView::sceneEvent(QEvent* event)
{
    // Re-implemented in order to allows fixing event-related bugs in patch releases.
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
#if QT_VERSION >= 0x040400
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
    d->page->d->client = d; // set the page client

    QSize size = geometry().size().toSize();
    page->setViewportSize(size);

    QWebFrame* mainFrame = d->page->mainFrame();

    connect(mainFrame, SIGNAL(titleChanged(const QString&)),
            this, SIGNAL(titleChanged(const QString&)));
    connect(mainFrame, SIGNAL(iconChanged()),
            this, SIGNAL(iconChanged()));
    connect(mainFrame, SIGNAL(urlChanged(const QUrl&)),
            this, SIGNAL(urlChanged(const QUrl&)));
    connect(d->page, SIGNAL(loadStarted()),
            this, SIGNAL(loadStarted()));
    connect(d->page, SIGNAL(loadProgress(int)),
            this, SIGNAL(loadProgress(int)));
    connect(d->page, SIGNAL(loadFinished(bool)),
            this, SLOT(_q_doLoadFinished(bool)));
    connect(d->page, SIGNAL(statusBarMessage(const QString&)),
            this, SIGNAL(statusBarMessage(const QString&)));
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
    \since 4.5
    \brief the zoom factor for the view
*/

void QGraphicsWebView::setZoomFactor(qreal factor)
{
    if (factor == page()->mainFrame()->zoomFactor())
        return;

    page()->mainFrame()->setZoomFactor(factor);
    emit zoomFactorChanged();
}

qreal QGraphicsWebView::zoomFactor() const
{
    return page()->mainFrame()->zoomFactor();
}

/*! \reimp
*/
void QGraphicsWebView::updateGeometry()
{
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
    //if (d->page)
    //    d->page->event(ev);
    //Just remove this line below when the code above is working
    Q_UNUSED(ev);
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

#include "moc_qgraphicswebview.cpp"
