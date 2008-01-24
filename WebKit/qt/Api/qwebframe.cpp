/*
    Copyright (C) 2007 Trolltech ASA
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

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
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
#include "ResourceRequest.h"
#include "SelectionController.h"
#include "PlatformScrollBar.h"
#include "SubstituteData.h"

#include "markup.h"
#include "RenderTreeAsText.h"
#include "Element.h"
#include "Document.h"
#include "DragData.h"
#include "RenderObject.h"
#include "GraphicsContext.h"
#include "PlatformScrollBar.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"

#include "bindings/runtime.h"
#include "bindings/runtime_root.h"
#include "kjs_proxy.h"
#include "kjs_window.h"
#include "kjs_binding.h"
#include "ExecState.h"
#include "object.h"

#include "wtf/HashMap.h"

#include <qdebug.h>
#include <qevent.h>
#include <qpainter.h>
#if QT_VERSION >= 0x040400
#include <qnetworkrequest.h>
#else
#include "qwebnetworkinterface.h"
#endif
#include <qregion.h>

using namespace WebCore;

void QWebFramePrivate::init(QWebFrame *qframe, WebCore::Page *webcorePage, QWebFrameData *frameData)
{
    q = qframe;

    frameLoaderClient = new FrameLoaderClientQt();
    frame = new Frame(webcorePage, frameData->ownerElement, frameLoaderClient);
    frameLoaderClient->setFrame(qframe, frame.get());

    frameView = new FrameView(frame.get());
    frameView->deref();
    frameView->setQWebFrame(qframe);
    if (!frameData->allowsScrolling)
        frameView->setScrollbarsMode(ScrollbarAlwaysOff);
    if (frameData->marginWidth != -1)
        frameView->setMarginWidth(frameData->marginWidth);
    if (frameData->marginHeight != -1)
        frameView->setMarginHeight(frameData->marginHeight);

    frame->setView(frameView.get());
    frame->init();

    QObject::connect(q, SIGNAL(hoveringOverLink(const QString&, const QString&, const QString&)),
                     page, SIGNAL(hoveringOverLink(const QString&, const QString&, const QString&)));
}

WebCore::PlatformScrollbar *QWebFramePrivate::horizontalScrollBar() const
{
    Q_ASSERT(frameView);
    return frameView->horizontalScrollBar();
}

WebCore::PlatformScrollbar *QWebFramePrivate::verticalScrollBar() const
{
    Q_ASSERT(frameView);
    return frameView->verticalScrollBar();
}

/*!
    \class QWebFrame
    \since 4.4
    \brief The QWebFrame class represents a frame in a web page.

    QWebFrame represents a frame inside a web page. Each QWebPage
    object contains at least one frame, the mainFrame(). Additional
    frames will be created for HTML &lt;frame&gt; or &lt;iframe&gt;
    elements.

    QWebFrame objects are created and controlled by the web page. You
    can connect to the web pages frameCreated() signal to find out
    about creation of new frames.

    \sa QWebPage
*/

QWebFrame::QWebFrame(QWebPage *parent, QWebFrameData *frameData)
    : QObject(parent)
    , d(new QWebFramePrivate)
{
    d->page = parent;
    d->init(this, parent->d->page, frameData);

    if (!frameData->url.isEmpty()) {
        ResourceRequest request(frameData->url, frameData->referrer);
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
    Q_ASSERT(d->frame == 0);
    Q_ASSERT(d->frameView == 0);
    delete d;
}

/*!
  Make \a object available under \a name from within the frames
  JavaScript context. The \a object will be inserted as a child of the
  frames window object.

  Qt properties will be exposed as JavaScript properties and slots as
  JavaScript methods.
*/
void QWebFrame::addToJSWindowObject(const QString &name, QObject *object)
{
      KJS::JSLock lock;
      KJS::Window *window = KJS::Window::retrieveWindow(d->frame.get());
      KJS::Bindings::RootObject *root = d->frame->bindingRootObject();
      if (!window) {
          qDebug() << "Warning: couldn't get window object";
          return;
      }

      KJS::JSObject *runtimeObject =
        KJS::Bindings::Instance::createRuntimeObject(KJS::Bindings::Instance::QtLanguage,
                                                     object, root);

      window->put(window->globalExec(), KJS::Identifier((const KJS::UChar *) name.constData(), name.length()), runtimeObject);
}

/*!
  returns the markup (HTML) contained in the current frame.
*/
QString QWebFrame::markup() const
{
    if (!d->frame->document())
        return QString();
    return createMarkup(d->frame->document());
}

/*!
  returns the content of this frame as plain text.
*/
QString QWebFrame::innerText() const
{
    if (d->frameView->layoutPending())
        d->frameView->layout();

    Element *documentElement = d->frame->document()->documentElement();
    return documentElement->innerText();
}

/*!
  returns a dump of the rendering tree. Mainly useful for debugging html.
*/
QString QWebFrame::renderTreeDump() const
{
    if (d->frameView->layoutPending())
        d->frameView->layout();

    return externalRepresentation(d->frame->renderer());
}

/*!
  The title of the frame as defined by the HTML &lt;title&gt;
  element.
*/
QString QWebFrame::title() const
{
    if (d->frame->document())
        return d->frame->loader()->documentLoader()->title();
    else return QString();
}

/*!
  The url of this frame.
*/
QUrl QWebFrame::url() const
{
    return d->frame->loader()->url();
}

/*!
  The icon associated with this frame.
*/
QPixmap QWebFrame::icon() const
{
    String url = d->frame->loader()->url().string();

    Image* image = 0;
    if (!url.isEmpty()) {
        image = iconDatabase()->iconForPageURL(url, IntSize(16, 16));
    }

    if (!image || image->isNull()) {
        image = iconDatabase()->defaultIcon(IntSize(16, 16));
    }

    if (!image) {
        return QPixmap();
    }

    QPixmap *icon = image->getPixmap();
    if (!icon) {
        return QPixmap();
    }
    return *icon;
}

/*!
  The name of this frame as defined by the parent frame.
*/
QString QWebFrame::name() const
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
  Load \a url into this frame.
*/
void QWebFrame::load(const QUrl &url)
{
#if QT_VERSION < 0x040400
    load(QWebNetworkRequest(url));
#else
    load(QNetworkRequest(url));
#endif
}

#if QT_VERSION < 0x040400
/*!
  Load network request \a req into this frame.
*/
void QWebFrame::load(const QWebNetworkRequest &req)
{
    if (d->parentFrame())
        d->page->d->insideOpenCall = true;

    QUrl url = req.url();
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

    if (!postData.isEmpty()) {
        WTF::RefPtr<WebCore::FormData> formData = new WebCore::FormData(postData.constData(), postData.size());
        request.setHTTPBody(formData);
    }

    d->frame->loader()->load(request);

    if (d->parentFrame())
        d->page->d->insideOpenCall = false;
}

#else

/*!
  Load network request \a req into this frame. Use the method specified in \a
  operation. \a body is optional and is only used for POST operations.
*/
void QWebFrame::load(const QNetworkRequest &req,
                     QNetworkAccessManager::Operation operation,
                     const QByteArray &body)
{
    if (d->parentFrame())
        d->page->d->insideOpenCall = true;

    QUrl url = req.url();

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

    if (!body.isEmpty()) {
        WTF::RefPtr<WebCore::FormData> formData = new WebCore::FormData(body.constData(), body.size());
        request.setHTTPBody(formData);
    }

    d->frame->loader()->load(request);

    if (d->parentFrame())
        d->page->d->insideOpenCall = false;
}
#endif

/*!
  Sets the content of this frame to \a html. \a baseUrl is optional and used to resolve relative
  URLs in the document.
*/
void QWebFrame::setHtml(const QString &html, const QUrl &baseUrl)
{
    KURL kurl(baseUrl);
    WebCore::ResourceRequest request(kurl);
    WTF::RefPtr<WebCore::SharedBuffer> data = new WebCore::SharedBuffer(reinterpret_cast<const uchar *>(html.unicode()), html.length() * 2);
    WebCore::SubstituteData substituteData(data, WebCore::String("text/html"), WebCore::String("utf-16"), kurl);
    d->frame->loader()->load(request, substituteData);
}

/*!
  \overload
*/
void QWebFrame::setHtml(const QByteArray &html, const QUrl &baseUrl)
{
    setContent(html, QString(), baseUrl);
}

/*!
  Sets the content of this frame to \a data assuming \a mimeType. If
  \a mimeType is not specified it defaults to 'text/html'.  \a baseUrl
  us optional and used to resolve relative URLs in the document.
*/
void QWebFrame::setContent(const QByteArray &data, const QString &mimeType, const QUrl &baseUrl)
{
    KURL kurl(baseUrl);
    WebCore::ResourceRequest request(kurl);
    WTF::RefPtr<WebCore::SharedBuffer> buffer = new WebCore::SharedBuffer(data.constData(), data.length());
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
*/
QWebFrame *QWebFrame::parentFrame() const
{
    return d->parentFrame();
}

/*!
  Returns a list of all frames that are direct children of this frame.
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
  \property QWebFrame::verticalScrollBarPolicy

  This property defines the vertical scrollbar policy.

  \sa Qt::ScrollBarPolicy
*/
Qt::ScrollBarPolicy QWebFrame::verticalScrollBarPolicy() const
{
    return (Qt::ScrollBarPolicy) d->frameView->vScrollbarMode();
}

void QWebFrame::setVerticalScrollBarPolicy(Qt::ScrollBarPolicy policy)
{
    Q_ASSERT((int)ScrollbarAuto == (int)Qt::ScrollBarAsNeeded);
    Q_ASSERT((int)ScrollbarAlwaysOff == (int)Qt::ScrollBarAlwaysOff);
    Q_ASSERT((int)ScrollbarAlwaysOn == (int)Qt::ScrollBarAlwaysOn);
    d->frameView->setVScrollbarMode((ScrollbarMode)policy);
}

/*!
  \property QWebFrame::horizontalScrollBarPolicy

  This property defines the horizontal scrollbar policy.

  \sa Qt::ScrollBarPolicy
*/
Qt::ScrollBarPolicy QWebFrame::horizontalScrollBarPolicy() const
{
    return (Qt::ScrollBarPolicy) d->frameView->hScrollbarMode();
}

void QWebFrame::setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy policy)
{
    d->frameView->setHScrollbarMode((ScrollbarMode)policy);
}

/*!
  Render the frame into \a painter clipping to \a clip.
*/
void QWebFrame::render(QPainter *painter, const QRegion &clip)
{
    if (!d->frameView || !d->frame->renderer())
        return;

    layout();

    GraphicsContext ctx(painter);
    QVector<QRect> vector = clip.rects();
    for (int i = 0; i < vector.size(); ++i) 
        d->frameView->paint(&ctx, vector.at(i));
}

/*!
  Ensure that the content of the frame and all subframes are correctly layouted.
*/
void QWebFrame::layout()
{
    if (!d->frameView)
        return;

    d->frameView->layoutIfNeededRecursive();
}

/*!
  returns the position of the frame relative to it's parent frame.
*/
QPoint QWebFrame::pos() const
{
    Q_ASSERT(d->frameView);
    return d->pos();
}

/*!
  return the geometry of the frame relative to it's parent frame.
*/
QRect QWebFrame::geometry() const
{
    Q_ASSERT(d->frameView);
    return d->frameView->frameGeometry();
}

/*!
  Evaluate JavaScript defined by \a scriptSource using this frame as context.
*/
QString QWebFrame::evaluateJavaScript(const QString& scriptSource)
{
    KJSProxy *proxy = d->frame->scriptProxy();
    QString rc;
    if (proxy) {
        KJS::JSValue *v = proxy->evaluate(String(), 0, scriptSource);
        if (v) {
            rc = String(v->toString(proxy->globalObject()->globalExec()));
        }
    }
    return rc;
}

WebCore::Frame* QWebFramePrivate::core(QWebFrame* webFrame)
{
    return webFrame->d->frame.get();
}

QWebFrame* QWebFramePrivate::kit(WebCore::Frame* coreFrame)
{
    return static_cast<FrameLoaderClientQt*>(coreFrame->loader()->client())->webFrame();
}


/*!
  \fn void QWebFrame::cleared()

  This signal is emitted whenever the content of the frame is cleared
  (e.g. before starting a new load).
*/

/*!
  \fn void QWebFrame::loadDone(bool ok)

  This signal is emitted when the frame is completely loaded. \a ok will indicate whether the load
  was successful or any error occurred.
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

  This signal is emitted whenever the \a url of the frame changes.

  \sa url()
*/

/*!
  \fn void QWebFrame::hoveringOverLink(const QString &link, const QString &title, const QString &textContent)

  This signal is emitted whenever the mouse cursor is hovering over a
  link. It can be used to display information about the link in
  e.g. the status bar. The signal arguments consist of the \a link destination, the \a title and the
  link text as \a textContent .
*/


/*!
  \fn void QWebFrame::loadStarted()

  This signal is emitted when a new load of the frame is started.
*/

/*!
  \fn void QWebFrame::loadFinished()
  
  This signal is emitted when a load of the frame is finished.
*/

/*!
  \fn void QWebFrame::initialLayoutComplete()

  This signal is emitted when the first (initial) layout of the frame
  has happened. This is the earliest time something can be shown on
  the screen.
*/
    
/*!
  \fn void QWebFrame::iconLoaded()

  This signal is emitted when the icon ("favicon") associated with the frame has been loaded.
*/
