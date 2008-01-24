/*
    Copyright (C) 2007 Trolltech ASA

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
#include "qwebview.h"
#include "qwebframe.h"
#include "qevent.h"
#include "qpainter.h"

class QWebViewPrivate
{
public:
    QWebPage *page;
};

/*!
    \class QWebView
    \since 4.4
    \brief The QWebView class provides a widget that is used to view and edit web documents.

    QWebView is the main widget component of the QtWebKit web browsing module.
*/

/*!
    Constructs an empty QWebView with parent \a parent.
*/
QWebView::QWebView(QWidget *parent)
    : QWidget(parent)
{
    d = new QWebViewPrivate;
    d->page = 0;

    QPalette pal = palette();
    pal.setBrush(QPalette::Background, Qt::white);

    setAttribute(Qt::WA_OpaquePaintEvent);

    setPalette(pal);
    setAcceptDrops(true);

    setMouseTracking(true);
    setFocusPolicy(Qt::WheelFocus);
}

/*!
    Destructor.
*/
QWebView::~QWebView()
{
    if (d->page && d->page->parent() == this)
        delete d->page;
    delete d;
}

/*!
    Returns a pointer to the underlying web page.

    \sa setPage()
*/
QWebPage *QWebView::page() const
{
    if (!d->page) {
        QWebView *that = const_cast<QWebView *>(this);
        that->setPage(new QWebPage(that));
    }
    return d->page;
}

/*!
    Makes \a page the new web page of the web view.

    The parent QObject of the provided page remains the owner
    of the object. If the current document is a child of the web
    view, then it is deleted.

    \sa page()
*/
void QWebView::setPage(QWebPage *page)
{
    if (d->page == page)
        return;
    if (d->page) {
        if (d->page->parent() == this) {
            delete d->page;
        } else {
            d->page->disconnect(this);
        }
    }
    d->page = page;
    if (d->page) {
        d->page->setView(this);
        // #### connect signals
        QWebFrame *mainFrame = d->page->mainFrame();
        connect(mainFrame, SIGNAL(loadStarted()),
                this, SIGNAL(loadStarted()));
        connect(mainFrame, SIGNAL(loadFinished()),
                this, SIGNAL(loadFinished()));
        connect(mainFrame, SIGNAL(titleChanged(const QString&)),
                this, SIGNAL(titleChanged(const QString&)));
        connect(mainFrame, SIGNAL(iconLoaded()),
                this, SIGNAL(iconLoaded()));
        connect(mainFrame, SIGNAL(urlChanged(const QUrl &)),
                this, SIGNAL(urlChanged(const QUrl &)));

        connect(d->page, SIGNAL(loadProgressChanged(int)),
                this, SIGNAL(loadProgressChanged(int)));
        connect(d->page, SIGNAL(statusBarTextChanged(const QString &)),
                this, SIGNAL(statusBarTextChanged(const QString &)));
    }
    update();
}

/*!
    Downloads the specified \a url and displays it.
*/
void QWebView::load(const QUrl &url)
{
    page()->mainFrame()->load(url);
}

#if QT_VERSION < 0x040400
void QWebView::load(const QWebNetworkRequest &request)
#else
void QWebView::load(const QNetworkRequest &request,
                    QNetworkAccessManager::Operation operation,
                    const QByteArray &body)
#endif
{
    page()->mainFrame()->load(request
#if QT_VERSION >= 0x040400
                              , operation, body
#endif
                             );
}

/*!
    Sets the content of the web view to the specified \a html.

    External objects referenced in the HTML document are located relative to \a baseUrl.
*/
void QWebView::setHtml(const QString &html, const QUrl &baseUrl)
{
    page()->mainFrame()->setHtml(html, baseUrl);
}

/*!
    Sets the content of the web view to the specified \a html.

    External objects referenced in the HTML document are located relative to \a baseUrl.
*/
void QWebView::setHtml(const QByteArray &html, const QUrl &baseUrl)
{
    page()->mainFrame()->setHtml(html, baseUrl);
}

/*!
    Sets the content of the web view to the specified content \a data. If the \a mimeType argument
    is empty it is assumed that the content is HTML.

    External objects referenced in the HTML document are located relative to \a baseUrl.
*/
void QWebView::setContent(const QByteArray &data, const QString &mimeType, const QUrl &baseUrl)
{
    page()->mainFrame()->setContent(data, mimeType, baseUrl);
}

/*!
    Returns a pointer to the view's history of navigated web pages.

    It is equivalent to

    \snippet doc/src/snippets/code/src.3rdparty.webkit.WebKit.qt.Api.qwebview.cpp 0
*/
QWebHistory *QWebView::history() const
{
    return page()->history();
}

/*!
    Returns a pointer to the view/page specific settings object.

    It is equivalent to

    \snippet doc/src/snippets/code/src.3rdparty.webkit.WebKit.qt.Api.qwebview.cpp 1
*/
QWebSettings *QWebView::settings() const
{
    return page()->settings();
}

/*!
  \property QWebView::title
  \brief the title of the web page currently viewed.
*/
QString QWebView::title() const
{
    if (d->page)
        return d->page->mainFrame()->title();
    return QString();
}

/*!
    \property QWebView::url
    \brief the url of the web page currently viewed.
*/
QUrl QWebView::url() const
{
    if (d->page)
        return d->page->mainFrame()->url();
    return QUrl();
}

/*!
    \property QWebView::icon
    \brief the icon associated with the web page currently viewed.
*/
QPixmap QWebView::icon() const
{
    if (d->page)
        return d->page->mainFrame()->icon();
    return QPixmap();
}

/*!
    \property QWebView::selectedText
    \brief the text currently selected.
*/
QString QWebView::selectedText() const
{
    if (d->page)
        return d->page->selectedText();
    return QString();
}

/*!
    Returns a pointer to a QAction that encapsulates the specified web action \a action.
*/
QAction *QWebView::action(QWebPage::WebAction action) const
{
    return page()->action(action);
}

/*!
    Triggers the specified \a action. If it is a checkable action the specified \a checked state is assumed.

    The following example triggers the copy action and therefore copies any selected text to the clipboard.

    \snippet doc/src/snippets/code/src.3rdparty.webkit.WebKit.qt.Api.qwebview.cpp 2
*/
void QWebView::triggerAction(QWebPage::WebAction action, bool checked)
{
    page()->triggerAction(action, checked);
}

/*!
    \property QWebView::modified
    \brief whether the document was modified by the user

    Parts of HTML documents can be editable for example through the \c{contenteditable} attribute on
    HTML elements.
*/
bool QWebView::isModified() const
{
    if (d->page)
        return d->page->isModified();
    return false;
}

Qt::TextInteractionFlags QWebView::textInteractionFlags() const
{
    // ### FIXME (add to page)
    return Qt::TextInteractionFlags();
}

/*!
    \property QWebView::textInteractionFlags
    \brief how the view should handle user input

    Specifies how the user can interact with the text on the page.
*/

void QWebView::setTextInteractionFlags(Qt::TextInteractionFlags flags)
{
    Q_UNUSED(flags)
    // ### FIXME (add to page)
}

/*!
    \reimp
*/
QSize QWebView::sizeHint() const
{
    return QSize(800, 600); // ####...
}

/*!
    Convenience slot that stops loading the document.

    It is equivalent to

    \snippet doc/src/snippets/code/src.3rdparty.webkit.WebKit.qt.Api.qwebview.cpp 3
*/
void QWebView::stop()
{
    if (d->page)
        d->page->triggerAction(QWebPage::Stop);
}

/*!
    Convenience slot that loads the previous document in the list of
    documents built by navigating links. Does nothing if there is no
    previous document.

    It is equivalent to

    \snippet doc/src/snippets/code/src.3rdparty.webkit.WebKit.qt.Api.qwebview.cpp 4
*/
void QWebView::backward()
{
    if (d->page)
        d->page->triggerAction(QWebPage::GoBack);
}

/*!
    Convenience slot that loads the next document in the list of
    documents built by navigating links. Does nothing if there is no
    next document.

    It is equivalent to

    \snippet doc/src/snippets/code/src.3rdparty.webkit.WebKit.qt.Api.qwebview.cpp 5
*/
void QWebView::forward()
{
    if (d->page)
        d->page->triggerAction(QWebPage::GoForward);
}

/*!
    Reloads the current document.
*/
void QWebView::reload()
{
    if (d->page)
        d->page->triggerAction(QWebPage::Reload);
}

/*! \reimp
*/
void QWebView::resizeEvent(QResizeEvent *e)
{
    if (d->page)
        d->page->setViewportSize(e->size());
}

/*! \reimp
*/
void QWebView::paintEvent(QPaintEvent *ev)
{
    if (!d->page)
        return;
#ifdef QWEBKIT_TIME_RENDERING
    QTime time;
    time.start();
#endif

    QWebFrame *frame = d->page->mainFrame();
    QPainter p(this);

    frame->render(&p, ev->region());

#ifdef    QWEBKIT_TIME_RENDERING
    int elapsed = time.elapsed();
    qDebug()<<"paint event on "<<ev->region()<<", took to render =  "<<elapsed;
#endif
}

/*!
    This function is called whenever WebKit wants to create a new window, for example as a result of
    a JavaScript request to open a document in a new window.
*/
QWebView *QWebView::createWindow()
{
    return 0;
}

/*! \reimp
*/
void QWebView::mouseMoveEvent(QMouseEvent* ev)
{
    if (d->page)
        d->page->event(ev);
}

/*! \reimp
*/
void QWebView::mousePressEvent(QMouseEvent* ev)
{
    if (d->page)
        d->page->event(ev);
}

/*! \reimp
*/
void QWebView::mouseDoubleClickEvent(QMouseEvent* ev)
{
    if (d->page)
        d->page->event(ev);
}

/*! \reimp
*/
void QWebView::mouseReleaseEvent(QMouseEvent* ev)
{
    if (d->page)
        d->page->event(ev);
}

/*! \reimp
*/
void QWebView::contextMenuEvent(QContextMenuEvent* ev)
{
    if (d->page)
        d->page->event(ev);
}

/*! \reimp
*/
void QWebView::wheelEvent(QWheelEvent* ev)
{
    if (d->page)
        d->page->event(ev);

    if (!ev->isAccepted())
        return QWidget::wheelEvent(ev);
}

/*! \reimp
*/
void QWebView::keyPressEvent(QKeyEvent* ev)
{
    if (d->page)
        d->page->event(ev);
    if (!ev->isAccepted())
        QWidget::keyPressEvent(ev);
}

/*! \reimp
*/
void QWebView::keyReleaseEvent(QKeyEvent* ev)
{
    if (d->page)
        d->page->event(ev);
    if (!ev->isAccepted())
        QWidget::keyReleaseEvent(ev);
}

/*! \reimp
*/
void QWebView::focusInEvent(QFocusEvent* ev)
{
    if (d->page)
        d->page->event(ev);
    QWidget::focusInEvent(ev);
}

/*! \reimp
*/
void QWebView::focusOutEvent(QFocusEvent* ev)
{
    QWidget::focusOutEvent(ev);
    if (d->page)
        d->page->event(ev);
}

/*! \reimp
*/
void QWebView::dragEnterEvent(QDragEnterEvent* ev)
{
#ifndef QT_NO_DRAGANDDROP
    if (d->page)
        d->page->event(ev);
#endif
}

/*! \reimp
*/
void QWebView::dragLeaveEvent(QDragLeaveEvent* ev)
{
#ifndef QT_NO_DRAGANDDROP
    if (d->page)
        d->page->event(ev);
#endif
}

/*! \reimp
*/
void QWebView::dragMoveEvent(QDragMoveEvent* ev)
{
#ifndef QT_NO_DRAGANDDROP
    if (d->page)
        d->page->event(ev);
#endif
}

/*! \reimp
*/
void QWebView::dropEvent(QDropEvent* ev)
{
#ifndef QT_NO_DRAGANDDROP
    if (d->page)
        d->page->event(ev);
#endif
}

/*! \reimp
*/
bool QWebView::focusNextPrevChild(bool next)
{
    if (d->page && d->page->focusNextPrevChild(next))
        return true;
    return QWidget::focusNextPrevChild(next);
}

/*!
  \fn void QWebView::titleChanged(const QString &title)

  This signal is emitted whenever the \a title of the main frame changes.

  \sa title()
*/

/*!
  \fn void QWebView::urlChanged(const QUrl &url)

  This signal is emitted whenever the \a url of the main frame changes.

  \sa url()
*/

/*!
    \fn void QWebView::statusBarTextChanged(const QString& text)

    This signal is emitted when the statusbar \a text is changed by the page.
*/

/*!
    \fn void QWebView::iconLoaded()

    This signal is emitted whenever the icon of the page is loaded or changes.
*/

/*!
    \fn void QWebView::loadStarted()

    This signal is emitted when a new load of the frame is started.
*/

/*!
    \fn void QWebView::loadFinished()

    This signal is emitted when a load of the frame is finished.
*/

/*!
    \fn void QWebView::selectionChanged()

    This signal is emitted whenever the selection changes.
*/

/*!
    \fn void QWebView::loadProgressChanged(int progress)

    This signal is emitted when the global progress status changes.
    The current value is provided by \a progress in percent.
    It accumulates changes from all the child frames.
*/
