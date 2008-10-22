/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2007 Staikos Computing Services Inc.

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
#include "qwebframe.h"
#include "qwebpage.h"
#include "qwebpage_p.h"
#include "qwebframe_p.h"

#include "DocumentLoader.h"
#include "FocusController.h"
#include "FrameLoaderClientQt.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "IconDatabase.h"
#include "Page.h"
#include "PutPropertySlot.h"
#include "ResourceRequest.h"
#include "RenderView.h"
#include "SelectionController.h"
#include "Scrollbar.h"
#include "PrintContext.h"
#include "SubstituteData.h"

#include "markup.h"
#include "RenderTreeAsText.h"
#include "Element.h"
#include "Document.h"
#include "DragData.h"
#include "RenderView.h"
#include "GraphicsContext.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"

#include "runtime.h"
#include "runtime_object.h"
#include "runtime_root.h"
#include "JSDOMWindow.h"
#include "qt_instance.h"
#include "ScriptController.h"
#include "JSDOMBinding.h"
#include "ExecState.h"
#include "JSLock.h"
#include "JSObject.h"
#include "qt_runtime.h"

#include "wtf/HashMap.h"

#include <qdebug.h>
#include <qevent.h>
#include <qfileinfo.h>
#include <qpainter.h>
#if QT_VERSION >= 0x040400
#include <qnetworkrequest.h>
#else
#include "qwebnetworkinterface.h"
#endif
#include <qregion.h>
#include <qprinter.h>

using namespace WebCore;

// from text/qfont.cpp
QT_BEGIN_NAMESPACE
extern Q_GUI_EXPORT int qt_defaultDpi();
QT_END_NAMESPACE

void QWebFramePrivate::init(QWebFrame *qframe, WebCore::Page *webcorePage, QWebFrameData *frameData)
{
    q = qframe;

    allowsScrolling = frameData->allowsScrolling;
    marginWidth = frameData->marginWidth;
    marginHeight = frameData->marginHeight;

    frameLoaderClient = new FrameLoaderClientQt();
    RefPtr<Frame> newFrame = Frame::create(webcorePage, frameData->ownerElement, frameLoaderClient);
    frame = newFrame.get();
    if (frameData->ownerElement)
        frame->ref(); // balanced by adoptRef in FrameLoaderClientQt::createFrame
    frameLoaderClient->setFrame(qframe, frame);
    frame->init();
}

WebCore::Scrollbar* QWebFramePrivate::horizontalScrollBar() const
{
    if (!frame->view())
        return 0;
    return frame->view()->horizontalScrollbar();
}

WebCore::Scrollbar* QWebFramePrivate::verticalScrollBar() const
{
    if (!frame->view())
        return 0;
    return frame->view()->verticalScrollbar();
}

void QWebFramePrivate::updateBackground()
{
    WebCore::FrameView *view = frame->view();
    if (!view)
        return;
    QBrush brush = page->palette().brush(QPalette::Background);
    if (brush.style() == Qt::SolidPattern) {
        view->setBaseBackgroundColor(brush.color());
        if (!brush.color().alpha())
            view->setTransparent(true);
    }
}

/*!
    \class QWebFrame
    \since 4.4
    \brief The QWebFrame class represents a frame in a web page.

    QWebFrame represents a frame inside a web page. Each QWebPage
    object contains at least one frame, the main frame, obtained using
    QWebPage::mainFrame(). Additional frames will be created for HTML
    \c{<frame>} or \c{<iframe>} elements.

    A frame can be loaded using load() or setUrl(). Alternatively, if you have
    the HTML content readily available, you can use setHtml() instead.

    The page() function returns a pointer to the web page object. See
    \l{Elements of QWebView} for an explanation of how web
    frames are related to a web page and web view.

    The title of an HTML frame can be accessed with the title() property.
    Additionally, a frame may also specify an icon, which can be accessed
    using the icon() property. If the title or the icon changes, the
    corresponding titleChanged() and iconChanged() signals will be emitted.
    The zoomFactor() property can be used to change the overall size
    of the content displayed in the frame.

    QWebFrame objects are created and controlled by the web page. You
    can connect to the web page's \l{QWebPage::}{frameCreated()} signal
    to be notified when a new frame is created.

    The hitTestContent() function can be used to programmatically examine the
    contents of a frame.

    A QWebFrame can be printed onto a QPrinter using the print() function.
    This function is marked as a slot and can be conveniently connected to
    \l{QPrintPreviewDialog}'s \l{QPrintPreviewDialog::}{paintRequested()}
    signal.

    \sa QWebPage
*/

QWebFrame::QWebFrame(QWebPage *parent, QWebFrameData *frameData)
    : QObject(parent)
    , d(new QWebFramePrivate)
{
    d->page = parent;
    d->init(this, parent->d->page, frameData);

    if (!frameData->url.isEmpty()) {
        WebCore::ResourceRequest request(frameData->url, frameData->referrer);
        d->frame->loader()->load(request, frameData->name);
    }
}

QWebFrame::QWebFrame(QWebFrame *parent, QWebFrameData *frameData)
    : QObject(parent)
    , d(new QWebFramePrivate)
{
    d->page = parent->d->page;
    d->init(this, parent->d->page->d->page, frameData);
}

QWebFrame::~QWebFrame()
{
    if (d->frame && d->frame->loader() && d->frame->loader()->client())
        static_cast<FrameLoaderClientQt*>(d->frame->loader()->client())->m_webFrame = 0;

    delete d;
}

/*!
    Make \a object available under \a name from within the frame's JavaScript
    context. The \a object will be inserted as a child of the frame's window
    object.

    Qt properties will be exposed as JavaScript properties and slots as
    JavaScript methods.

    If you want to ensure that your QObjects remain accessible after loading a
    new URL, you should add them in a slot connected to the
    javaScriptWindowObjectCleared() signal.
*/
void QWebFrame::addToJavaScriptWindowObject(const QString &name, QObject *object)
{
      JSC::JSLock lock(false);
      JSDOMWindow *window = toJSDOMWindow(d->frame);
      JSC::Bindings::RootObject *root = d->frame->script()->bindingRootObject();
      if (!window) {
          qDebug() << "Warning: couldn't get window object";
          return;
      }

      JSC::ExecState* exec = window->globalExec();

      JSC::JSObject *runtimeObject =
        JSC::Bindings::Instance::createRuntimeObject(exec, JSC::Bindings::QtInstance::getQtInstance(object, root));

      JSC::PutPropertySlot slot;
      window->put(exec, JSC::Identifier(exec, (const UChar *) name.constData(), name.length()), runtimeObject, slot);
}

/*!
    Returns the frame's content, converted to HTML.

    \sa setHtml(), toPlainText()
*/
QString QWebFrame::toHtml() const
{
    if (!d->frame->document())
        return QString();
    return createMarkup(d->frame->document());
}

/*!
    Returns the content of this frame converted to plain text.

    \sa toHtml()
*/
QString QWebFrame::toPlainText() const
{
    if (d->frame->view() && d->frame->view()->layoutPending())
        d->frame->view()->layout();

    Element *documentElement = d->frame->document()->documentElement();
    return documentElement->innerText();
}

/*!
    Returns a dump of the rendering tree. This is mainly useful for debugging
    html.
*/
QString QWebFrame::renderTreeDump() const
{
    if (d->frame->view() && d->frame->view()->layoutPending())
        d->frame->view()->layout();

    return externalRepresentation(d->frame->contentRenderer());
}

/*!
    \property QWebFrame::title
    \brief the title of the frame as defined by the HTML &lt;title&gt; element

    \sa titleChanged()
*/

QString QWebFrame::title() const
{
    if (d->frame->document())
        return d->frame->loader()->documentLoader()->title();
    else return QString();
}

static inline QUrl ensureAbsoluteUrl(const QUrl &url)
{
    if (!url.isRelative())
        return url;

    return QUrl::fromLocalFile(QFileInfo(url.toLocalFile()).absoluteFilePath());
}

/*!
    \property QWebFrame::url
    \brief the url of the frame currently viewed

    \sa urlChanged()
*/

void QWebFrame::setUrl(const QUrl &url)
{
    d->frame->loader()->begin(ensureAbsoluteUrl(url));
    d->frame->loader()->end();
    load(ensureAbsoluteUrl(url));
}

QUrl QWebFrame::url() const
{
    return d->frame->loader()->url();
}

/*!
    \property QWebFrame::icon
    \brief the icon associated with this frame

    \sa iconChanged(), QWebSettings::iconForUrl()
*/

QIcon QWebFrame::icon() const
{
    return QWebSettings::iconForUrl(d->frame->loader()->url());
}

/*!
  The name of this frame as defined by the parent frame.
*/
QString QWebFrame::frameName() const
{
    return d->frame->tree()->name();
}

/*!
  The web page that contains this frame.
*/
QWebPage *QWebFrame::page() const
{
    return d->page;
}

/*!
  Loads \a url into this frame.

  \note The view remains the same until enough data has arrived to display the new \a url.

  \sa setUrl(), setHtml(), setContent()
*/
void QWebFrame::load(const QUrl &url)
{
#if QT_VERSION < 0x040400
    load(QWebNetworkRequest(ensureAbsoluteUrl(url)));
#else
    load(QNetworkRequest(ensureAbsoluteUrl(url)));
#endif
}

#if QT_VERSION < 0x040400
/*!
  Loads a network request, \a req, into this frame.

  \note The view remains the same until enough data has arrived to display the new url.
*/
void QWebFrame::load(const QWebNetworkRequest &req)
{
    if (d->parentFrame())
        d->page->d->insideOpenCall = true;

    QUrl url = ensureAbsoluteUrl(req.url());
    QHttpRequestHeader httpHeader = req.httpHeader();
    QByteArray postData = req.postData();

    WebCore::ResourceRequest request(url);

    QString method = httpHeader.method();
    if (!method.isEmpty())
        request.setHTTPMethod(method);

    QList<QPair<QString, QString> > values = httpHeader.values();
    for (int i = 0; i < values.size(); ++i) {
        const QPair<QString, QString> &val = values.at(i);
        request.addHTTPHeaderField(val.first, val.second);
    }

    if (!postData.isEmpty())
        request.setHTTPBody(WebCore::FormData::create(postData.constData(), postData.size()));

    d->frame->loader()->load(request);

    if (d->parentFrame())
        d->page->d->insideOpenCall = false;
}

#else

/*!
  Loads a network request, \a req, into this frame, using the method specified in \a
  operation.

  \a body is optional and is only used for POST operations.

  \note The view remains the same until enough data has arrived to display the new \a url.

  \sa setUrl()
*/
void QWebFrame::load(const QNetworkRequest &req,
                     QNetworkAccessManager::Operation operation,
                     const QByteArray &body)
{
    if (d->parentFrame())
        d->page->d->insideOpenCall = true;

    QUrl url = ensureAbsoluteUrl(req.url());

    WebCore::ResourceRequest request(url);

    switch (operation) {
        case QNetworkAccessManager::HeadOperation:
            request.setHTTPMethod("HEAD");
            break;
        case QNetworkAccessManager::GetOperation:
            request.setHTTPMethod("GET");
            break;
        case QNetworkAccessManager::PutOperation:
            request.setHTTPMethod("PUT");
            break;
        case QNetworkAccessManager::PostOperation:
            request.setHTTPMethod("POST");
            break;
        case QNetworkAccessManager::UnknownOperation:
            // eh?
            break;
    }

    QList<QByteArray> httpHeaders = req.rawHeaderList();
    for (int i = 0; i < httpHeaders.size(); ++i) {
        const QByteArray &headerName = httpHeaders.at(i);
        request.addHTTPHeaderField(QString::fromLatin1(headerName), QString::fromLatin1(req.rawHeader(headerName)));
    }

    if (!body.isEmpty())
        request.setHTTPBody(WebCore::FormData::create(body.constData(), body.size()));

    d->frame->loader()->load(request);

    if (d->parentFrame())
        d->page->d->insideOpenCall = false;
}
#endif

/*!
  Sets the content of this frame to \a html. \a baseUrl is optional and used to resolve relative
  URLs in the document.

  When using this method WebKit assumes that external resources such as JavaScript programs or style
  sheets are encoded in UTF-8 unless otherwise specified. For example, the encoding of an external
  script can be specified through the charset attribute of the HTML script tag. It is also possible
  for the encoding to be specified by web server.

  \sa toHtml()
*/
void QWebFrame::setHtml(const QString &html, const QUrl &baseUrl)
{
    KURL kurl(baseUrl);
    WebCore::ResourceRequest request(kurl);
    const QByteArray utf8 = html.toUtf8();
    WTF::RefPtr<WebCore::SharedBuffer> data = WebCore::SharedBuffer::create(utf8.constData(), utf8.length());
    WebCore::SubstituteData substituteData(data, WebCore::String("text/html"), WebCore::String("utf-8"), kurl);
    d->frame->loader()->load(request, substituteData);
}

/*!
  Sets the content of this frame to the specified content \a data. If the \a mimeType argument
  is empty it is currently assumed that the content is HTML but in future versions we may introduce
  auto-detection.

  External objects referenced in the content are located relative to \a baseUrl.

  \sa toHtml()
*/
void QWebFrame::setContent(const QByteArray &data, const QString &mimeType, const QUrl &baseUrl)
{
    KURL kurl(baseUrl);
    WebCore::ResourceRequest request(kurl);
    WTF::RefPtr<WebCore::SharedBuffer> buffer = WebCore::SharedBuffer::create(data.constData(), data.length());
    QString actualMimeType = mimeType;
    if (actualMimeType.isEmpty())
        actualMimeType = QLatin1String("text/html");
    WebCore::SubstituteData substituteData(buffer, WebCore::String(actualMimeType), WebCore::String(), kurl);
    d->frame->loader()->load(request, substituteData);
}


/*!
  Returns the parent frame of this frame, or 0 if the frame is the web pages
  main frame.

  This is equivalent to qobject_cast<QWebFrame*>(frame->parent()).

  \sa childFrames()
*/
QWebFrame *QWebFrame::parentFrame() const
{
    return d->parentFrame();
}

/*!
  Returns a list of all frames that are direct children of this frame.

  \sa parentFrame()
*/
QList<QWebFrame*> QWebFrame::childFrames() const
{
    QList<QWebFrame*> rc;
    if (d->frame) {
        FrameTree *tree = d->frame->tree();
        for (Frame *child = tree->firstChild(); child; child = child->tree()->nextSibling()) {
            FrameLoader *loader = child->loader();
            FrameLoaderClientQt *client = static_cast<FrameLoaderClientQt*>(loader->client());
            if (client)
                rc.append(client->webFrame());
        }

    }
    return rc;
}

/*!
    Returns the scrollbar policy for the scrollbar defined by \a orientation.
*/
Qt::ScrollBarPolicy QWebFrame::scrollBarPolicy(Qt::Orientation orientation) const
{
    if (orientation == Qt::Horizontal)
        return d->horizontalScrollBarPolicy;
    return d->verticalScrollBarPolicy;
}

/*!
    Sets the scrollbar policy for the scrollbar defined by \a orientation to \a policy.
*/
void QWebFrame::setScrollBarPolicy(Qt::Orientation orientation, Qt::ScrollBarPolicy policy)
{
    Q_ASSERT((int)ScrollbarAuto == (int)Qt::ScrollBarAsNeeded);
    Q_ASSERT((int)ScrollbarAlwaysOff == (int)Qt::ScrollBarAlwaysOff);
    Q_ASSERT((int)ScrollbarAlwaysOn == (int)Qt::ScrollBarAlwaysOn);

    if (orientation == Qt::Horizontal) {
        d->horizontalScrollBarPolicy = policy;
        if (d->frame->view())
            d->frame->view()->setHorizontalScrollbarMode((ScrollbarMode)policy);
    } else {
        d->verticalScrollBarPolicy = policy;
        if (d->frame->view())
            d->frame->view()->setVerticalScrollbarMode((ScrollbarMode)policy);
    }
}

/*!
  Sets the current \a value for the scrollbar with orientation \a orientation.

  The scrollbar forces the \a value to be within the legal range: minimum <= value <= maximum.

  Changing the value also updates the thumb position.

  \sa scrollBarMinimum(), scrollBarMaximum()
*/
void QWebFrame::setScrollBarValue(Qt::Orientation orientation, int value)
{
    Scrollbar *sb;
    sb = (orientation == Qt::Horizontal) ? d->horizontalScrollBar() : d->verticalScrollBar();
    if (sb) {
        if (value < 0)
            value = 0;
        else if (value > scrollBarMaximum(orientation))
            value = scrollBarMaximum(orientation);
        sb->setValue(value);
    }
}

/*!
  Returns the current value for the scrollbar with orientation \a orientation, or 0
  if no scrollbar is found for \a orientation.

  \sa scrollBarMinimum(), scrollBarMaximum()
*/
int QWebFrame::scrollBarValue(Qt::Orientation orientation) const
{
    Scrollbar *sb;
    sb = (orientation == Qt::Horizontal) ? d->horizontalScrollBar() : d->verticalScrollBar();
    if (sb) {
        return sb->value();
    }
    return 0;
}

/*!
  Returns the maximum value for the scrollbar with orientation \a orientation, or 0
  if no scrollbar is found for \a orientation.

  \sa scrollBarMinimum()
*/
int QWebFrame::scrollBarMaximum(Qt::Orientation orientation) const
{
    Scrollbar *sb;
    sb = (orientation == Qt::Horizontal) ? d->horizontalScrollBar() : d->verticalScrollBar();
    if (sb)
        return sb->maximum();
    return 0;
}

/*!
  Returns the minimum value for the scrollbar with orientation \a orientation.

  The minimum value is always 0.

  \sa scrollBarMaximum()
*/
int QWebFrame::scrollBarMinimum(Qt::Orientation orientation) const
{
    return 0;
}

/*!
  \since 4.5
  Scrolls the frame \a dx pixels to the right and \a dy pixels downward. Both
  \a dx and \a dy may be negative.
  
  \sa QWebFrame::scrollOffset
*/

void QWebFrame::scroll(int dx, int dy) const
{
    if (!d->frame->view())
        return;
    
    d->frame->view()->scrollBy(IntSize(dx, dy));
}

/*!
  \property QWebFrame::scrollOffset
  \brief The offset from the start this frame is currently scrolled to.
*/

QSize QWebFrame::scrollOffset() const
{
    if (!d->frame->view())
        return QSize(0,0);

    return d->frame->view()->scrollOffset();
}

void QWebFrame::setScrollOffset(const QSize &offset) const
{
    QSize current = scrollOffset();
    int dx = offset.width() - current.width();
    int dy = offset.height() - current.height();
    scroll(dx, dy);
}

/*!
  Render the frame into \a painter clipping to \a clip.

  \sa print()
*/
void QWebFrame::render(QPainter *painter, const QRegion &clip)
{
    if (!d->frame->view() || !d->frame->contentRenderer())
        return;

    d->frame->view()->layoutIfNeededRecursive();

    GraphicsContext ctx(painter);
    QVector<QRect> vector = clip.rects();
    WebCore::FrameView* view = d->frame->view();
    for (int i = 0; i < vector.size(); ++i) {
        if (i > 0) {
            painter->save();
            painter->setClipRect(vector.at(i), Qt::IntersectClip);
        }

        view->paint(&ctx, vector.at(i));

        if (i > 0)
            painter->restore();
    }
}

/*!
  Render the frame into \a painter.
*/
void QWebFrame::render(QPainter *painter)
{
    if (!d->frame->view() || !d->frame->contentRenderer())
        return;

    d->frame->view()->layoutIfNeededRecursive();

    GraphicsContext ctx(painter);
    WebCore::FrameView* view = d->frame->view();
    view->paint(&ctx, view->frameRect());
}

/*!
    \property QWebFrame::textSizeMultiplier
    \brief the scaling factor for all text in the frame
    \obsolete

    Use setZoomFactor instead, in combination with the ZoomTextOnly attribute in
    QWebSettings.

    \note Setting this property also enables the ZoomTextOnly attribute in
    QWebSettings.
*/

void QWebFrame::setTextSizeMultiplier(qreal factor)
{
    d->frame->setZoomFactor(factor, /*isTextOnly*/true);
}

qreal QWebFrame::textSizeMultiplier() const
{
    return d->frame->zoomFactor();
}

/*!
    \property QWebFrame::zoomFactor
    \since 4.5
    \brief the zoom factor for the frame
*/

void QWebFrame::setZoomFactor(qreal factor)
{
    d->frame->setZoomFactor(factor, d->frame->isZoomFactorTextOnly());
}

qreal QWebFrame::zoomFactor() const
{
    return d->frame->zoomFactor();
}

/*!
    Returns the position of the frame relative to it's parent frame.
*/
QPoint QWebFrame::pos() const
{
    if (!d->frame->view())
        return QPoint();

    return d->frame->view()->frameRect().topLeft();
}

/*!
    Return the geometry of the frame relative to it's parent frame.
*/
QRect QWebFrame::geometry() const
{
    if (!d->frame->view())
        return QRect();
    return d->frame->view()->frameRect();
}

/*!
    \property QWebFrame::contentsSize
    \brief the size of the contents in this frame
*/
QSize QWebFrame::contentsSize() const
{
    FrameView *view = d->frame->view();
    if (!view)
        return QSize();
    return QSize(view->contentsWidth(), view->contentsHeight());
}

/*!
    Performs a hit test on the frame contents at the given position \a pos and returns the hit test result.
*/
QWebHitTestResult QWebFrame::hitTestContent(const QPoint &pos) const
{
    if (!d->frame->view() || !d->frame->contentRenderer())
        return QWebHitTestResult();

    HitTestResult result = d->frame->eventHandler()->hitTestResultAtPoint(d->frame->view()->windowToContents(pos), /*allowShadowContent*/ false);
    return QWebHitTestResult(new QWebHitTestResultPrivate(result));
}

/*! \reimp
*/
bool QWebFrame::event(QEvent *e)
{
    return QObject::event(e);
}

#ifndef QT_NO_PRINTER
/*!
    Prints the frame to the given \a printer.

    \sa render()
*/
void QWebFrame::print(QPrinter *printer) const
{
    const qreal zoomFactorX = printer->logicalDpiX() / qt_defaultDpi();
    const qreal zoomFactorY = printer->logicalDpiY() / qt_defaultDpi();

    PrintContext printContext(d->frame);
    float pageHeight = 0;

    QRect qprinterRect = printer->pageRect();

    IntRect pageRect(0, 0,
                     int(qprinterRect.width() / zoomFactorX),
                     int(qprinterRect.height() / zoomFactorY));

    printContext.begin(pageRect.width());

    printContext.computePageRects(pageRect, /*headerHeight*/0, /*footerHeight*/0, /*userScaleFactor*/1.0, pageHeight);

    int docCopies;
    int pageCopies;
    if (printer->collateCopies() == true){
        docCopies = 1;
        pageCopies = printer->numCopies();
    } else {
        docCopies = printer->numCopies();
        pageCopies = 1;
    }

    int fromPage = printer->fromPage();
    int toPage = printer->toPage();
    bool ascending = true;

    if (fromPage == 0 && toPage == 0) {
        fromPage = 1;
        toPage = printContext.pageCount();
    }
    // paranoia check
    fromPage = qMax(1, fromPage);
    toPage = qMin(printContext.pageCount(), toPage);

    if (printer->pageOrder() == QPrinter::LastPageFirst) {
        int tmp = fromPage;
        fromPage = toPage;
        toPage = tmp;
        ascending = false;
    }

    QPainter painter(printer);
    painter.scale(zoomFactorX, zoomFactorY);
    GraphicsContext ctx(&painter);

    for (int i = 0; i < docCopies; ++i) {
        int page = fromPage;
        while (true) {
            for (int j = 0; j < pageCopies; ++j) {
                if (printer->printerState() == QPrinter::Aborted
                    || printer->printerState() == QPrinter::Error) {
                    printContext.end();
                    return;
                }
                printContext.spoolPage(ctx, page - 1, pageRect.width());
                if (j < pageCopies - 1)
                    printer->newPage();
            }

            if (page == toPage)
                break;

            if (ascending)
                ++page;
            else
                --page;

            printer->newPage();
        }

        if ( i < docCopies - 1)
            printer->newPage();
    }

    printContext.end();
}
#endif // QT_NO_PRINTER

/*!
    Evaluate JavaScript defined by \a scriptSource using this frame as context.

    \sa addToJavaScriptWindowObject(), javaScriptWindowObjectCleared()
*/
QVariant QWebFrame::evaluateJavaScript(const QString& scriptSource)
{
    ScriptController *proxy = d->frame->script();
    QVariant rc;
    if (proxy) {
        JSC::JSValuePtr v = proxy->evaluate(String(), 1, scriptSource);
        if (v) {
            int distance = 0;
            rc = JSC::Bindings::convertValueToQVariant(proxy->globalObject()->globalExec(), v, QMetaType::Void, &distance);
        }
    }
    return rc;
}

WebCore::Frame* QWebFramePrivate::core(QWebFrame* webFrame)
{
    return webFrame->d->frame;
}

QWebFrame* QWebFramePrivate::kit(WebCore::Frame* coreFrame)
{
    return static_cast<FrameLoaderClientQt*>(coreFrame->loader()->client())->webFrame();
}


/*!
    \fn void QWebFrame::javaScriptWindowObjectCleared()

    This signal is emitted whenever the global window object of the JavaScript
    environment is cleared, e.g., before starting a new load.

    If you intend to add QObjects to a QWebFrame using
    addToJavaScriptWindowObject(), you should add them in a slot connected
    to this signal. This ensures that your objects remain accessible when
    loading new URLs.
*/

/*!
    \fn void QWebFrame::provisionalLoad()
    \internal
*/

/*!
    \fn void QWebFrame::titleChanged(const QString &title)

    This signal is emitted whenever the title of the frame changes.
    The \a title string specifies the new title.

    \sa title()
*/

/*!
    \fn void QWebFrame::urlChanged(const QUrl &url)

    This signal is emitted with the URL of the frame when the frame's title is
    received. The new URL is specified by \a url.

    \sa url()
*/

/*!
    \fn void QWebFrame::initialLayoutCompleted()

    This signal is emitted when the frame is laid out the first time.
    This is the first time you will see contents displayed on the frame.

    \note A frame can be laid out multiple times.
*/

/*!
  \fn void QWebFrame::iconChanged()

  This signal is emitted when the icon ("favicon") associated with the frame
  has been loaded.

  \sa icon()
*/

/*!
    \class QWebHitTestResult
    \since 4.4
    \brief The QWebHitTestResult class provides information about the web
    page content after a hit test.

    QWebHitTestResult is returned by QWebFrame::hitTestContent() to provide
    information about the content of the web page at the specified position.
*/

/*!
    \internal
*/
QWebHitTestResult::QWebHitTestResult(QWebHitTestResultPrivate *priv)
    : d(priv)
{
}

QWebHitTestResultPrivate::QWebHitTestResultPrivate(const WebCore::HitTestResult &hitTest)
    : isContentEditable(false)
    , isContentSelected(false)
{
    if (!hitTest.innerNode())
        return;
    pos = hitTest.point();
    boundingRect = hitTest.boundingBox();
    title = hitTest.title();
    linkText = hitTest.textContent();
    linkUrl = hitTest.absoluteLinkURL();
    linkTitle = hitTest.titleDisplayString();
    alternateText = hitTest.altDisplayString();
    imageUrl = hitTest.absoluteImageURL();
    innerNonSharedNode = hitTest.innerNonSharedNode();
    WebCore::Image *img = hitTest.image();
    if (img) {
        QPixmap *pix = img->nativeImageForCurrentFrame();
        if (pix)
            pixmap = *pix;
    }
    WebCore::Frame *wframe = hitTest.targetFrame();
    if (wframe)
        linkTargetFrame = QWebFramePrivate::kit(wframe);

    isContentEditable = hitTest.isContentEditable();
    isContentSelected = hitTest.isSelected();

    if (innerNonSharedNode && innerNonSharedNode->document()
        && innerNonSharedNode->document()->frame())
        frame = QWebFramePrivate::kit(innerNonSharedNode->document()->frame());
}

/*!
    Constructs a null hit test result.
*/
QWebHitTestResult::QWebHitTestResult()
    : d(0)
{
}

/*!
    Constructs a hit test result from \a other.
*/
QWebHitTestResult::QWebHitTestResult(const QWebHitTestResult &other)
    : d(0)
{
    if (other.d)
        d = new QWebHitTestResultPrivate(*other.d);
}

/*!
    Assigns the \a other hit test result to this.
*/
QWebHitTestResult &QWebHitTestResult::operator=(const QWebHitTestResult &other)
{
    if (this != &other) {
        if (other.d) {
            if (!d)
                d = new QWebHitTestResultPrivate;
            *d = *other.d;
        } else {
            delete d;
            d = 0;
        }
    }
    return *this;
}

/*!
    Destructor.
*/
QWebHitTestResult::~QWebHitTestResult()
{
    delete d;
}

/*!
    Returns true if the hit test result is null; otherwise returns false.
*/
bool QWebHitTestResult::isNull() const
{
    return !d;
}

/*!
    Returns the position where the hit test occured.
*/
QPoint QWebHitTestResult::pos() const
{
    if (!d)
        return QPoint();
    return d->pos;
}

/*!
    \since 4.5
    Returns the bounding box of the element.
*/
QRect QWebHitTestResult::boundingRect() const
{
    if (!d)
        return QRect();
    return d->boundingRect;
}

/*!
    Returns the title of the nearest enclosing HTML element.
*/
QString QWebHitTestResult::title() const
{
    if (!d)
        return QString();
    return d->title;
}

/*!
    Returns the text of the link.
*/
QString QWebHitTestResult::linkText() const
{
    if (!d)
        return QString();
    return d->linkText;
}

/*!
    Returns the url to which the link points to.
*/
QUrl QWebHitTestResult::linkUrl() const
{
    if (!d)
        return QUrl();
    return d->linkUrl;
}

/*!
    Returns the title of the link.
*/
QUrl QWebHitTestResult::linkTitle() const
{
    if (!d)
        return QUrl();
    return d->linkTitle;
}

/*!
    Returns the frame that will load the link if it is activated.
*/
QWebFrame *QWebHitTestResult::linkTargetFrame() const
{
    if (!d)
        return 0;
    return d->linkTargetFrame;
}

/*!
    Returns the alternate text of the element. This corresponds to the HTML alt attribute.
*/
QString QWebHitTestResult::alternateText() const
{
    if (!d)
        return QString();
    return d->alternateText;
}

/*!
    Returns the url of the image.
*/
QUrl QWebHitTestResult::imageUrl() const
{
    if (!d)
        return QUrl();
    return d->imageUrl;
}

/*!
    Returns a QPixmap containing the image. A null pixmap is returned if the
    element being tested is not an image.
*/
QPixmap QWebHitTestResult::pixmap() const
{
    if (!d)
        return QPixmap();
    return d->pixmap;
}

/*!
    Returns true if the content is editable by the user; otherwise returns false.
*/
bool QWebHitTestResult::isContentEditable() const
{
    if (!d)
        return false;
    return d->isContentEditable;
}

/*!
    Returns true if the content tested is part of the selection; otherwise returns false.
*/
bool QWebHitTestResult::isContentSelected() const
{
    if (!d)
        return false;
    return d->isContentSelected;
}

/*!
    Returns the frame the hit test was executed in.
*/
QWebFrame *QWebHitTestResult::frame() const
{
    if (!d)
        return 0;
    return d->frame;
}

