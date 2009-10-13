/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2008 Holger Hans Peter Freyther
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
#include "qwebview.h"

#include "QWebPageClient.h"
#include "qwebframe.h"
#include "qwebpage_p.h"

#include "qbitmap.h"
#include "qevent.h"
#include "qpainter.h"
#include "qprinter.h"
#include "qdir.h"
#include "qfile.h"
#if defined(Q_WS_X11)
#include <QX11Info>
#endif

class QWebViewPrivate : public QWebPageClient {
public:
    QWebViewPrivate(QWebView *view)
        : view(view)
        , page(0)
        , renderHints(QPainter::TextAntialiasing)
    {
        Q_ASSERT(view);
    }

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

    void _q_pageDestroyed();

    QWebView *view;
    QWebPage *page;

    QPainter::RenderHints renderHints;
};

void QWebViewPrivate::scroll(int dx, int dy, const QRect& rectToScroll)
{
    view->scroll(qreal(dx), qreal(dy), rectToScroll);
}

void QWebViewPrivate::update(const QRect & dirtyRect)
{
    view->update(dirtyRect);
}

void QWebViewPrivate::setInputMethodEnabled(bool enable)
{
    view->setAttribute(Qt::WA_InputMethodEnabled, enable);
}
#if QT_VERSION >= 0x040600
void QWebViewPrivate::setInputMethodHint(Qt::InputMethodHint hint, bool enable)
{
    if (enable)
        view->setInputMethodHints(view->inputMethodHints() | hint);
    else
        view->setInputMethodHints(view->inputMethodHints() & ~hint);
}
#endif
#ifndef QT_NO_CURSOR
QCursor QWebViewPrivate::cursor() const
{
    return view->cursor();
}

void QWebViewPrivate::updateCursor(const QCursor& cursor)
{
    view->setCursor(cursor);
}
#endif

QPalette QWebViewPrivate::palette() const
{
    return view->palette();
}

int QWebViewPrivate::screenNumber() const
{
#if defined(Q_WS_X11)
    if (view)
        return view->x11Info().screen();
#endif

    return 0;
}

QWidget* QWebViewPrivate::ownerWidget() const
{
    return view;
}

QObject* QWebViewPrivate::pluginParent() const
{
    return view;
}

void QWebViewPrivate::_q_pageDestroyed()
{
    page = 0;
    view->setPage(0);
}

/*!
    \class QWebView
    \since 4.4
    \brief The QWebView class provides a widget that is used to view and edit
    web documents.
    \ingroup advanced

    \inmodule QtWebKit

    QWebView is the main widget component of the QtWebKit web browsing module.
    It can be used in various applications to display web content live from the
    Internet.

    The image below shows QWebView previewed in \QD with the Trolltech website.

    \image qwebview-url.png

    A web site can be loaded onto QWebView with the load() function. Like all
    Qt widgets, the show() function must be invoked in order to display
    QWebView. The snippet below illustrates this:

    \snippet webkitsnippets/simple/main.cpp Using QWebView

    Alternatively, setUrl() can also be used to load a web site. If you have
    the HTML content readily available, you can use setHtml() instead.

    The loadStarted() signal is emitted when the view begins loading. The
    loadProgress() signal, on the other hand, is emitted whenever an element of
    the web view completes loading, such as an embedded image, a script, etc.
    Finally, the loadFinished() signal is emitted when the view has loaded
    completely. It's argument - either \c true or \c false - indicates
    load success or failure.

    The page() function returns a pointer to the web page object. See
    \l{Elements of QWebView} for an explanation of how the web page
    is related to the view. To modify your web view's settings, you can access
    the QWebSettings object with the settings() function. With QWebSettings,
    you can change the default fonts, enable or disable features such as
    JavaScript and plugins.

    The title of an HTML document can be accessed with the title() property.
    Additionally, a web site may also specify an icon, which can be accessed
    using the icon() property. If the title or the icon changes, the corresponding
    titleChanged() and iconChanged() signals will be emitted. The
    textSizeMultiplier() property can be used to change the overall size of
    the text displayed in the web view.

    If you require a custom context menu, you can implement it by reimplementing
    \l{QWidget::}{contextMenuEvent()} and populating your QMenu with the actions
    obtained from pageAction(). More functionality such as reloading the view,
    copying selected text to the clipboard, or pasting into the view, is also
    encapsulated within the QAction objects returned by pageAction(). These
    actions can be programmatically triggered using triggerPageAction().
    Alternatively, the actions can be added to a toolbar or a menu directly.
    QWebView maintains the state of the returned actions but allows
    modification of action properties such as \l{QAction::}{text} or
    \l{QAction::}{icon}.

    A QWebView can be printed onto a QPrinter using the print() function.
    This function is marked as a slot and can be conveniently connected to
    \l{QPrintPreviewDialog}'s \l{QPrintPreviewDialog::}{paintRequested()}
    signal.

    If you want to provide support for web sites that allow the user to open
    new windows, such as pop-up windows, you can subclass QWebView and
    reimplement the createWindow() function.

    \section1 Elements of QWebView

    QWebView consists of other objects such as QWebFrame and QWebPage. The
    flowchart below shows these elements are related.

    \image qwebview-diagram.png

    \note It is possible to use QWebPage and QWebFrame, without using QWebView,
    if you do not require QWidget attributes. Nevertheless, QtWebKit depends
    on QtGui, so you should use a QApplication instead of QCoreApplication.

    \sa {Previewer Example}, {Web Browser}, {Form Extractor Example},
    {Google Chat Example}, {Fancy Browser Example}
*/

/*!
    Constructs an empty QWebView with parent \a parent.

    \sa load()
*/
QWebView::QWebView(QWidget *parent)
    : QWidget(parent)
{
    d = new QWebViewPrivate(this);

#if !defined(Q_WS_QWS) && !defined(Q_OS_SYMBIAN)
    setAttribute(Qt::WA_InputMethodEnabled);
#endif

    setAcceptDrops(true);

    setMouseTracking(true);
    setFocusPolicy(Qt::WheelFocus);
}

/*!
    Destroys the web view.
*/
QWebView::~QWebView()
{
    if (d->page)
        d->page->d->view = 0;

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
    view, it will be deleted.

    \sa page()
*/
void QWebView::setPage(QWebPage* page)
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
    if (d->page) {
        d->page->setView(this);
        d->page->d->client = d; // set the page client
        d->page->setPalette(palette());
        // #### connect signals
        QWebFrame *mainFrame = d->page->mainFrame();
        connect(mainFrame, SIGNAL(titleChanged(const QString&)),
                this, SIGNAL(titleChanged(const QString&)));
        connect(mainFrame, SIGNAL(iconChanged()),
                this, SIGNAL(iconChanged()));
        connect(mainFrame, SIGNAL(urlChanged(const QUrl &)),
                this, SIGNAL(urlChanged(const QUrl &)));

        connect(d->page, SIGNAL(loadStarted()),
                this, SIGNAL(loadStarted()));
        connect(d->page, SIGNAL(loadProgress(int)),
                this, SIGNAL(loadProgress(int)));
        connect(d->page, SIGNAL(loadFinished(bool)),
                this, SIGNAL(loadFinished(bool)));
        connect(d->page, SIGNAL(statusBarMessage(const QString &)),
                this, SIGNAL(statusBarMessage(const QString &)));
        connect(d->page, SIGNAL(linkClicked(const QUrl &)),
                this, SIGNAL(linkClicked(const QUrl &)));

        connect(d->page, SIGNAL(microFocusChanged()),
                this, SLOT(updateMicroFocus()));
        connect(d->page, SIGNAL(destroyed()),
                this, SLOT(_q_pageDestroyed()));
    }
    setAttribute(Qt::WA_OpaquePaintEvent, d->page);
    update();
}

/*!
    Returns a valid URL from a user supplied \a string if one can be deducted.
    In the case that is not possible, an invalid QUrl() is returned.

    \since 4.6

    Most applications that can browse the web, allow the user to input a URL
    in the form of a plain string. This string can be manually typed into
    a location bar, obtained from the clipboard, or passed in via command
    line arguments.

    When the string is not already a valid URL, a best guess is performed,
    making various web related assumptions.

    In the case the string corresponds to a valid file path on the system,
    a file:// URL is constructed, using QUrl::fromLocalFile().

    If that is not the case, an attempt is made to turn the string into a
    http:// or ftp:// URL. The latter in the case the string starts with
    'ftp'. The result is then passed through QUrl's tolerant parser, and
    in the case or success, a valid QUrl is returned, or else a QUrl().

    \section1 Examples:

    \list
    \o webkit.org becomes http://webkit.org
    \o ftp.webkit.org becomes ftp://ftp.webkit.org
    \o localhost becomes http://localhost
    \o /home/user/test.html becomes file:///home/user/test.html (if exists)
    \endlist

    \section2 Tips when dealing with URLs and strings:

    \list
    \o When creating a QString from a QByteArray or a char*, always use
      QString::fromUtf8().
    \o Do not use QUrl(string), nor QUrl::toString() anywhere where the URL might
       be used, such as in the location bar, as those functions loose data.
       Instead use QUrl::fromEncoded() and QUrl::toEncoded(), respectively.
    \endlist
 */
QUrl QWebView::guessUrlFromString(const QString &string)
{
    QString trimmedString = string.trimmed();

    // Check the most common case of a valid url with scheme and host first
    QUrl url = QUrl::fromEncoded(trimmedString.toUtf8(), QUrl::TolerantMode);
    if (url.isValid() && !url.scheme().isEmpty() && !url.host().isEmpty())
        return url;

    // Absolute files that exists
    if (QDir::isAbsolutePath(trimmedString) && QFile::exists(trimmedString))
        return QUrl::fromLocalFile(trimmedString);

    // If the string is missing the scheme or the scheme is not valid prepend a scheme
    QString scheme = url.scheme();
    if (scheme.isEmpty() || scheme.contains(QLatin1Char('.')) || scheme == QLatin1String("localhost")) {
        // Do not do anything for strings such as "foo", only "foo.com"
        int dotIndex = trimmedString.indexOf(QLatin1Char('.'));
        if (dotIndex != -1 || trimmedString.startsWith(QLatin1String("localhost"))) {
            const QString hostscheme = trimmedString.left(dotIndex).toLower();
            QByteArray scheme = (hostscheme == QLatin1String("ftp")) ? "ftp" : "http";
            trimmedString = QLatin1String(scheme) + QLatin1String("://") + trimmedString;
        }
        url = QUrl::fromEncoded(trimmedString.toUtf8(), QUrl::TolerantMode);
    }

    if (url.isValid())
        return url;

    return QUrl();
}

/*!
    Loads the specified \a url and displays it.

    \note The view remains the same until enough data has arrived to display the new \a url.

    \sa setUrl(), url(), urlChanged(), guessUrlFromString()
*/
void QWebView::load(const QUrl &url)
{
    page()->mainFrame()->load(url);
}

/*!
    \fn void QWebView::load(const QNetworkRequest &request, QNetworkAccessManager::Operation operation, const QByteArray &body)

    Loads a network request, \a request, using the method specified in \a operation.

    \a body is optional and is only used for POST operations.

    \note The view remains the same until enough data has arrived to display the new url.

    \sa url(), urlChanged()
*/

#if QT_VERSION < 0x040400 && !defined(qdoc)
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
void QWebView::setHtml(const QString &html, const QUrl &baseUrl)
{
    page()->mainFrame()->setHtml(html, baseUrl);
}

/*!
    Sets the content of the web view to the specified content \a data. If the \a mimeType argument
    is empty it is currently assumed that the content is HTML but in future versions we may introduce
    auto-detection.

    External objects referenced in the content are located relative to \a baseUrl.

    The \a data is loaded immediately; external objects are loaded asynchronously.

    \sa load(), setHtml(), QWebFrame::toHtml()
*/
void QWebView::setContent(const QByteArray &data, const QString &mimeType, const QUrl &baseUrl)
{
    page()->mainFrame()->setContent(data, mimeType, baseUrl);
}

/*!
    Returns a pointer to the view's history of navigated web pages.

    It is equivalent to

    \snippet webkitsnippets/qtwebkit_qwebview_snippet.cpp 0
*/
QWebHistory *QWebView::history() const
{
    return page()->history();
}

/*!
    Returns a pointer to the view/page specific settings object.

    It is equivalent to

    \snippet webkitsnippets/qtwebkit_qwebview_snippet.cpp 1

    \sa QWebSettings::globalSettings()
*/
QWebSettings *QWebView::settings() const
{
    return page()->settings();
}

/*!
    \property QWebView::title
    \brief the title of the web page currently viewed

    By default, this property contains an empty string.

    \sa titleChanged()
*/
QString QWebView::title() const
{
    if (d->page)
        return d->page->mainFrame()->title();
    return QString();
}

/*!
    \property QWebView::url
    \brief the url of the web page currently viewed

    Setting this property clears the view and loads the URL.

    By default, this property contains an empty, invalid URL.

    \sa load(), urlChanged()
*/

void QWebView::setUrl(const QUrl &url)
{
    page()->mainFrame()->setUrl(url);
}

QUrl QWebView::url() const
{
    if (d->page)
        return d->page->mainFrame()->url();
    return QUrl();
}

/*!
    \property QWebView::icon
    \brief the icon associated with the web page currently viewed

    By default, this property contains a null icon.

    \sa iconChanged(), QWebSettings::iconForUrl()
*/
QIcon QWebView::icon() const
{
    if (d->page)
        return d->page->mainFrame()->icon();
    return QIcon();
}

/*!
    \property QWebView::selectedText
    \brief the text currently selected

    By default, this property contains an empty string.

    \sa findText(), selectionChanged()
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
QAction *QWebView::pageAction(QWebPage::WebAction action) const
{
    return page()->action(action);
}

/*!
    Triggers the specified \a action. If it is a checkable action the specified
    \a checked state is assumed.

    The following example triggers the copy action and therefore copies any
    selected text to the clipboard.

    \snippet webkitsnippets/qtwebkit_qwebview_snippet.cpp 2

    \sa pageAction()
*/
void QWebView::triggerPageAction(QWebPage::WebAction action, bool checked)
{
    page()->triggerAction(action, checked);
}

/*!
    \property QWebView::modified
    \brief whether the document was modified by the user

    Parts of HTML documents can be editable for example through the
    \c{contenteditable} attribute on HTML elements.

    By default, this property is false.
*/
bool QWebView::isModified() const
{
    if (d->page)
        return d->page->isModified();
    return false;
}

/*
Qt::TextInteractionFlags QWebView::textInteractionFlags() const
{
    // ### FIXME (add to page)
    return Qt::TextInteractionFlags();
}
*/

/*
    \property QWebView::textInteractionFlags
    \brief how the view should handle user input

    Specifies how the user can interact with the text on the page.
*/

/*
void QWebView::setTextInteractionFlags(Qt::TextInteractionFlags flags)
{
    Q_UNUSED(flags)
    // ### FIXME (add to page)
}
*/

/*!
    \reimp
*/
QSize QWebView::sizeHint() const
{
    return QSize(800, 600); // ####...
}

/*!
    \property QWebView::zoomFactor
    \since 4.5
    \brief the zoom factor for the view
*/

void QWebView::setZoomFactor(qreal factor)
{
    page()->mainFrame()->setZoomFactor(factor);
}

qreal QWebView::zoomFactor() const
{
    return page()->mainFrame()->zoomFactor();
}

/*!
  \property QWebView::textSizeMultiplier
  \brief the scaling factor for all text in the frame
  \obsolete

  Use setZoomFactor instead, in combination with the
  ZoomTextOnly attribute in QWebSettings.

  \note Setting this property also enables the
  ZoomTextOnly attribute in QWebSettings.

  By default, this property contains a value of 1.0.
*/

/*!
    Sets the value of the multiplier used to scale the text in a Web page to
    the \a factor specified.
*/
void QWebView::setTextSizeMultiplier(qreal factor)
{
    page()->mainFrame()->setTextSizeMultiplier(factor);
}

/*!
    Returns the value of the multiplier used to scale the text in a Web page.
*/
qreal QWebView::textSizeMultiplier() const
{
    return page()->mainFrame()->textSizeMultiplier();
}

#if !defined(Q_OS_SYMBIAN)
/*!
    \property QWebView::renderHints
    \since 4.6
    \brief the default render hints for the view

    These hints are used to initialize QPainter before painting the web page.

    QPainter::TextAntialiasing is enabled by default.

    \sa QPainter::renderHints()
*/
#endif
QPainter::RenderHints QWebView::renderHints() const
{
    return d->renderHints;
}

void QWebView::setRenderHints(QPainter::RenderHints hints)
{
    if (hints == d->renderHints)
        return;
    d->renderHints = hints;
    update();
}

/*!
    If \a enabled is true, the render hint \a hint is enabled; otherwise it
    is disabled.

    \since 4.6
    \sa renderHints
*/
void QWebView::setRenderHint(QPainter::RenderHint hint, bool enabled)
{
    QPainter::RenderHints oldHints = d->renderHints;
    if (enabled)
        d->renderHints |= hint;
    else
        d->renderHints &= ~hint;
    if (oldHints != d->renderHints)
        update();
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

    \sa selectedText(), selectionChanged()
*/
bool QWebView::findText(const QString &subString, QWebPage::FindFlags options)
{
    if (d->page)
        return d->page->findText(subString, options);
    return false;
}

/*! \reimp
*/
bool QWebView::event(QEvent *e)
{
    if (d->page) {
#ifndef QT_NO_CONTEXTMENU
        if (e->type() == QEvent::ContextMenu) {
            if (!isEnabled())
                return false;
            QContextMenuEvent *event = static_cast<QContextMenuEvent *>(e);
            if (d->page->swallowContextMenuEvent(event)) {
                e->accept();
                return true;
            }
            d->page->updatePositionDependentActions(event->pos());
        } else
#endif // QT_NO_CONTEXTMENU
        if (e->type() == QEvent::ShortcutOverride) {
            d->page->event(e);
#ifndef QT_NO_CURSOR
#if QT_VERSION >= 0x040400
        } else if (e->type() == QEvent::CursorChange) {
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
#endif
#endif
        } else if (e->type() == QEvent::Leave)
            d->page->event(e);
    }

    return QWidget::event(e);
}

/*!
    Prints the main frame to the given \a printer.

    \sa QWebFrame::print(), QPrintPreviewDialog
*/
void QWebView::print(QPrinter *printer) const
{
#ifndef QT_NO_PRINTER
    page()->mainFrame()->print(printer);
#endif
}

/*!
    Convenience slot that stops loading the document.

    It is equivalent to

    \snippet webkitsnippets/qtwebkit_qwebview_snippet.cpp 3

    \sa reload(), pageAction(), loadFinished()
*/
void QWebView::stop()
{
    if (d->page)
        d->page->triggerAction(QWebPage::Stop);
}

/*!
    Convenience slot that loads the previous document in the list of documents
    built by navigating links. Does nothing if there is no previous document.

    It is equivalent to

    \snippet webkitsnippets/qtwebkit_qwebview_snippet.cpp 4

    \sa forward(), pageAction()
*/
void QWebView::back()
{
    if (d->page)
        d->page->triggerAction(QWebPage::Back);
}

/*!
    Convenience slot that loads the next document in the list of documents
    built by navigating links. Does nothing if there is no next document.

    It is equivalent to

    \snippet webkitsnippets/qtwebkit_qwebview_snippet.cpp 5

    \sa back(), pageAction()
*/
void QWebView::forward()
{
    if (d->page)
        d->page->triggerAction(QWebPage::Forward);
}

/*!
    Reloads the current document.

    \sa stop(), pageAction(), loadStarted()
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
    p.setRenderHints(d->renderHints);

    frame->render(&p, ev->region());

#ifdef    QWEBKIT_TIME_RENDERING
    int elapsed = time.elapsed();
    qDebug() << "paint event on " << ev->region() << ", took to render =  " << elapsed;
#endif
}

/*!
    This function is called whenever WebKit wants to create a new window of the given \a type, for example as a result of
    a JavaScript request to open a document in a new window.

    \sa QWebPage::createWindow()
*/
QWebView *QWebView::createWindow(QWebPage::WebWindowType type)
{
    Q_UNUSED(type)
    return 0;
}

/*! \reimp
*/
void QWebView::mouseMoveEvent(QMouseEvent* ev)
{
    if (d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }
}

/*! \reimp
*/
void QWebView::mousePressEvent(QMouseEvent* ev)
{
    if (d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }
}

/*! \reimp
*/
void QWebView::mouseDoubleClickEvent(QMouseEvent* ev)
{
    if (d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }
}

/*! \reimp
*/
void QWebView::mouseReleaseEvent(QMouseEvent* ev)
{
    if (d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }
}

#ifndef QT_NO_CONTEXTMENU
/*! \reimp
*/
void QWebView::contextMenuEvent(QContextMenuEvent* ev)
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
void QWebView::wheelEvent(QWheelEvent* ev)
{
    if (d->page) {
        const bool accepted = ev->isAccepted();
        d->page->event(ev);
        ev->setAccepted(accepted);
    }
}
#endif // QT_NO_WHEELEVENT

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
    else
        QWidget::focusInEvent(ev);
}

/*! \reimp
*/
void QWebView::focusOutEvent(QFocusEvent* ev)
{
    if (d->page)
        d->page->event(ev);
    else
        QWidget::focusOutEvent(ev);
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

/*!\reimp
*/
QVariant QWebView::inputMethodQuery(Qt::InputMethodQuery property) const
{
    if (d->page)
        return d->page->inputMethodQuery(property);
    return QVariant();
}

/*!\reimp
*/
void QWebView::inputMethodEvent(QInputMethodEvent *e)
{
    if (d->page)
       d->page->event(e);
}

/*!\reimp
*/
void QWebView::changeEvent(QEvent *e)
{
    if (d->page && e->type() == QEvent::PaletteChange)
        d->page->setPalette(palette());
    QWidget::changeEvent(e);
}

/*!
    \fn void QWebView::titleChanged(const QString &title)

    This signal is emitted whenever the \a title of the main frame changes.

    \sa title()
*/

/*!
    \fn void QWebView::urlChanged(const QUrl &url)

    This signal is emitted when the \a url of the view changes.

    \sa url(), load()
*/

/*!
    \fn void QWebView::statusBarMessage(const QString& text)

    This signal is emitted when the statusbar \a text is changed by the page.
*/

/*!
    \fn void QWebView::iconChanged()

    This signal is emitted whenever the icon of the page is loaded or changes.

    In order for icons to be loaded, you will need to set an icon database path
    using QWebSettings::setIconDatabasePath().

    \sa icon(), QWebSettings::setIconDatabasePath()
*/

/*!
    \fn void QWebView::loadStarted()

    This signal is emitted when a new load of the page is started.

    \sa loadProgress(), loadFinished()
*/

/*!
    \fn void QWebView::loadFinished(bool ok)

    This signal is emitted when a load of the page is finished.
    \a ok will indicate whether the load was successful or any error occurred.

    \sa loadStarted()
*/

/*!
    \fn void QWebView::selectionChanged()

    This signal is emitted whenever the selection changes.

    \sa selectedText()
*/

/*!
    \fn void QWebView::loadProgress(int progress)

    This signal is emitted every time an element in the web page
    completes loading and the overall loading progress advances.

    This signal tracks the progress of all child frames.

    The current value is provided by \a progress and scales from 0 to 100,
    which is the default range of QProgressBar.

    \sa loadStarted(), loadFinished()
*/

/*!
    \fn void QWebView::linkClicked(const QUrl &url)

    This signal is emitted whenever the user clicks on a link and the page's linkDelegationPolicy
    property is set to delegate the link handling for the specified \a url.

    \sa QWebPage::linkDelegationPolicy()
*/

#include "moc_qwebview.cpp"

