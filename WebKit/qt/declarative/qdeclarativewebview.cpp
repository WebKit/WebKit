/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

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


#include "qdeclarativewebview_p.h"

#include <QtCore/QDebug>
#include <QtCore/QEvent>
#include <QtCore/QFile>
#include <QtDeclarative/QDeclarativeContext>
#include <QtDeclarative/QDeclarativeEngine>
#include <QtDeclarative/qdeclarative.h>
#include <QtGui/QApplication>
#include <QtGui/QGraphicsSceneMouseEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPen>
#include "qwebelement.h"
#include "qwebframe.h"
#include "qwebpage.h"
#include "qwebsettings.h"

QT_BEGIN_NAMESPACE

class QDeclarativeWebViewPrivate {
public:
    QDeclarativeWebViewPrivate(QDeclarativeWebView* qq)
      : q(qq)
      , preferredwidth(0)
      , preferredheight(0)
      , progress(1.0)
      , status(QDeclarativeWebView::Null)
      , pending(PendingNone)
      , newWindowComponent(0)
      , newWindowParent(0)
      , rendering(true)
    {
    }

    QDeclarativeWebView* q;

    QUrl url; // page url might be different if it has not loaded yet
    GraphicsWebView* view;

    int preferredwidth, preferredheight;
    qreal progress;
    QDeclarativeWebView::Status status;
    QString statusText;
    enum { PendingNone, PendingUrl, PendingHtml, PendingContent } pending;
    QUrl pendingUrl;
    QString pendingString;
    QByteArray pendingData;
    mutable QDeclarativeWebSettings settings;
    QDeclarativeComponent* newWindowComponent;
    QDeclarativeItem* newWindowParent;

    static void windowObjectsAppend(QDeclarativeListProperty<QObject>* prop, QObject* o)
    {
        static_cast<QDeclarativeWebViewPrivate*>(prop->data)->windowObjects.append(o);
        static_cast<QDeclarativeWebViewPrivate*>(prop->data)->updateWindowObjects();
    }

    void updateWindowObjects();
    QObjectList windowObjects;

    bool rendering;
};

GraphicsWebView::GraphicsWebView(QDeclarativeWebView* parent)
    : QGraphicsWebView(parent)
    , parent(parent)
    , pressTime(400)
{
}

void GraphicsWebView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    pressPoint = event->pos();
    if (pressTime) {
        pressTimer.start(pressTime, this);
        parent->setKeepMouseGrab(false);
    } else {
        grabMouse();
        parent->setKeepMouseGrab(true);
    }
    QGraphicsWebView::mousePressEvent(event);

    QWebHitTestResult hit = page()->mainFrame()->hitTestContent(pressPoint.toPoint());
    if (hit.isContentEditable())
        parent->forceActiveFocus();
    setFocus();
}

void GraphicsWebView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsWebView::mouseReleaseEvent(event);
    pressTimer.stop();
    parent->setKeepMouseGrab(false);
    ungrabMouse();
}

void GraphicsWebView::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    QMouseEvent* me = new QMouseEvent(QEvent::MouseButtonDblClick, (event->pos() / parent->contentsScale()).toPoint(), event->button(), event->buttons(), 0);
    emit doubleClick(event->pos().x(), event->pos().y());
    delete me;
}

void GraphicsWebView::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == pressTimer.timerId()) {
        pressTimer.stop();
        grabMouse();
        parent->setKeepMouseGrab(true);
    }
}

void GraphicsWebView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (pressTimer.isActive()) {
        if ((event->pos() - pressPoint).manhattanLength() > QApplication::startDragDistance())
            pressTimer.stop();
    }
    if (parent->keepMouseGrab())
        QGraphicsWebView::mouseMoveEvent(event);
}

/*!
    \qmlclass WebView QDeclarativeWebView
    \ingroup qml-view-elements
    \since 4.7
    \brief The WebView item allows you to add Web content to a canvas.
    \inherits Item

    A WebView renders Web content based on a URL.

    This type is made available by importing the \c QtWebKit module:

    \bold{import QtWebKit 1.0}

    The WebView item includes no scrolling, scaling, toolbars, or other common browser
    components. These must be implemented around WebView. See the \l{QML Web Browser}
    example for a demonstration of this.

    The page to be displayed by the item is specified using the \l url property,
    and this can be changed to fetch and display a new page. While the page loads,
    the \l progress property is updated to indicate how much of the page has been
    loaded.

    \section1 Appearance

    If the width and height of the item is not set, they will dynamically adjust
    to a size appropriate for the content. This width may be large for typical
    online web pages, typically greater than 800 by 600 pixels.

    If the \l{Item::}{width} or \l{Item::}{height} is explictly set, the rendered Web site will be
    clipped, not scaled, to fit into the set dimensions.

    If the preferredWidth property is set, the width will be this amount or larger,
    usually laying out the Web content to fit the preferredWidth.

    The appearance of the content can be controlled to a certain extent by changing
    the settings.standardFontFamily property and other settings related to fonts.

    The page can be zoomed by calling the heuristicZoom() method, which performs a
    series of tests to determine whether zoomed content will be displayed in an
    appropriate way in the space allocated to the item.

    \section1 User Interaction and Navigation

    By default, certain mouse and touch events are delivered to other items in
    preference to the Web content. For example, when a scrolling view is created
    by placing a WebView in a Flickable, move events are delivered to the Flickable
    so that the user can scroll the page. This prevents the user from accidentally
    selecting text in a Web page instead of scrolling.

    The pressGrabTime property defines the time the user must touch or press a
    mouse button over the WebView before the Web content will receive the move
    events it needs to select text and images.

    When this item has keyboard focus, all keyboard input will be sent directly to
    the Web page within.

    When the navigates by clicking on links, the item records the pages visited
    in its internal history

    Because this item is designed to be used as a component in a browser, it
    exposes \l{Action}{actions} for \l back, \l forward, \l reload and \l stop.
    These can be triggered to change the current page displayed by the item.

    \section1 Example Usage

    \beginfloatright
    \inlineimage webview.png
    \endfloat

    The following example displays a scaled down Web page at a fixed size.

    \snippet doc/src/snippets/declarative/webview/webview.qml document

    \clearfloat

    \sa {declarative/modelviews/webview}{WebView example}, {demos/declarative/webbrowser}{Web Browser demo}
*/

/*!
    \internal
    \class QDeclarativeWebView
    \brief The QDeclarativeWebView class allows you to add web content to a QDeclarativeView.

    A WebView renders web content base on a URL.

    \image webview.png

    The item includes no scrolling, scaling,
    toolbars, etc., those must be implemented around WebView. See the WebBrowser example
    for a demonstration of this.

    A QDeclarativeWebView object can be instantiated in Qml using the tag \l WebView.
*/

QDeclarativeWebView::QDeclarativeWebView(QDeclarativeItem *parent) : QDeclarativeItem(parent)
{
    init();
}

QDeclarativeWebView::~QDeclarativeWebView()
{
    delete d;
}

void QDeclarativeWebView::init()
{
    d = new QDeclarativeWebViewPrivate(this);

    QWebSettings::enablePersistentStorage();

    setAcceptedMouseButtons(Qt::LeftButton);
    setFlag(QGraphicsItem::ItemHasNoContents, true);
    setClip(true);

    d->view = new GraphicsWebView(this);
    d->view->setResizesToContents(true);
    QWebPage* wp = new QDeclarativeWebPage(this);
    setPage(wp);
    connect(d->view, SIGNAL(geometryChanged()), this, SLOT(updateDeclarativeWebViewSize()));
    connect(d->view, SIGNAL(doubleClick(int, int)), this, SIGNAL(doubleClick(int, int)));
    connect(d->view, SIGNAL(scaleChanged()), this, SIGNAL(contentsScaleChanged()));
}

void QDeclarativeWebView::componentComplete()
{
    QDeclarativeItem::componentComplete();
    page()->setNetworkAccessManager(qmlEngine(this)->networkAccessManager());

    switch (d->pending) {
    case QDeclarativeWebViewPrivate::PendingUrl:
        setUrl(d->pendingUrl);
        break;
    case QDeclarativeWebViewPrivate::PendingHtml:
        setHtml(d->pendingString, d->pendingUrl);
        break;
    case QDeclarativeWebViewPrivate::PendingContent:
        setContent(d->pendingData, d->pendingString, d->pendingUrl);
        break;
    default:
        break;
    }
    d->pending = QDeclarativeWebViewPrivate::PendingNone;
    d->updateWindowObjects();
}

QDeclarativeWebView::Status QDeclarativeWebView::status() const
{
    return d->status;
}


/*!
    \qmlproperty real WebView::progress
    This property holds the progress of loading the current URL, from 0 to 1.

    If you just want to know when progress gets to 1, use
    WebView::onLoadFinished() or WebView::onLoadFailed() instead.
*/
qreal QDeclarativeWebView::progress() const
{
    return d->progress;
}

void QDeclarativeWebView::doLoadStarted()
{
    if (!d->url.isEmpty()) {
        d->status = Loading;
        emit statusChanged(d->status);
    }
    emit loadStarted();
}

void QDeclarativeWebView::doLoadProgress(int p)
{
    if (d->progress == p / 100.0)
        return;
    d->progress = p / 100.0;
    emit progressChanged();
}

void QDeclarativeWebView::pageUrlChanged()
{
    updateContentsSize();

    if ((d->url.isEmpty() && page()->mainFrame()->url() != QUrl(QLatin1String("about:blank")))
        || (d->url != page()->mainFrame()->url() && !page()->mainFrame()->url().isEmpty()))
    {
        d->url = page()->mainFrame()->url();
        if (d->url == QUrl(QLatin1String("about:blank")))
            d->url = QUrl();
        emit urlChanged();
    }
}

void QDeclarativeWebView::doLoadFinished(bool ok)
{
    if (ok) {
        d->status = d->url.isEmpty() ? Null : Ready;
        emit loadFinished();
    } else {
        d->status = Error;
        emit loadFailed();
    }
    emit statusChanged(d->status);
}

/*!
    \qmlproperty url WebView::url
    This property holds the URL to the page displayed in this item. It can be set,
    but also can change spontaneously (eg. because of network redirection).

    If the url is empty, the page is blank.

    The url is always absolute (QML will resolve relative URL strings in the context
    of the containing QML document).
*/
QUrl QDeclarativeWebView::url() const
{
    return d->url;
}

void QDeclarativeWebView::setUrl(const QUrl& url)
{
    if (url == d->url)
        return;

    if (isComponentComplete()) {
        d->url = url;
        updateContentsSize();
        QUrl seturl = url;
        if (seturl.isEmpty())
            seturl = QUrl(QLatin1String("about:blank"));

        Q_ASSERT(!seturl.isRelative());

        page()->mainFrame()->load(seturl);

        emit urlChanged();
    } else {
        d->pending = d->PendingUrl;
        d->pendingUrl = url;
    }
}

/*!
    \qmlproperty int WebView::preferredWidth
    This property holds the ideal width for displaying the current URL.
*/
int QDeclarativeWebView::preferredWidth() const
{
    return d->preferredwidth;
}

void QDeclarativeWebView::setPreferredWidth(int width)
{
    if (d->preferredwidth == width)
        return;
    d->preferredwidth = width;
    updateContentsSize();
    emit preferredWidthChanged();
}

/*!
    \qmlproperty int WebView::preferredHeight
    This property holds the ideal height for displaying the current URL.
    This only affects the area zoomed by heuristicZoom().
*/
int QDeclarativeWebView::preferredHeight() const
{
    return d->preferredheight;
}

void QDeclarativeWebView::setPreferredHeight(int height)
{
    if (d->preferredheight == height)
        return;
    d->preferredheight = height;
    updateContentsSize();
    emit preferredHeightChanged();
}

/*!
    \qmlmethod bool WebView::evaluateJavaScript(string scriptSource)

    Evaluates the \a scriptSource JavaScript inside the context of the
    main web frame, and returns the result of the last executed statement.

    Note that this JavaScript does \e not have any access to QML objects
    except as made available as windowObjects.
*/
QVariant QDeclarativeWebView::evaluateJavaScript(const QString& scriptSource)
{
    return this->page()->mainFrame()->evaluateJavaScript(scriptSource);
}

void QDeclarativeWebView::updateDeclarativeWebViewSize()
{
    QSizeF size = d->view->geometry().size() * contentsScale();
    setImplicitWidth(size.width());
    setImplicitHeight(size.height());
}

void QDeclarativeWebView::initialLayout()
{
    // nothing useful to do at this point
}

void QDeclarativeWebView::updateContentsSize()
{
    if (page()) {
        page()->setPreferredContentsSize(QSize(
            d->preferredwidth>0 ? d->preferredwidth : width(),
            d->preferredheight>0 ? d->preferredheight : height()));
    }
}

void QDeclarativeWebView::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QWebPage* webPage = page();
    if (newGeometry.size() != oldGeometry.size() && webPage) {
        QSize contentSize = webPage->preferredContentsSize();
        if (widthValid())
            contentSize.setWidth(width());
        if (heightValid())
            contentSize.setHeight(height());
        if (contentSize != webPage->preferredContentsSize())
            webPage->setPreferredContentsSize(contentSize);
    }
    QDeclarativeItem::geometryChanged(newGeometry, oldGeometry);
}

/*!
    \qmlproperty list<object> WebView::javaScriptWindowObjects

    A list of QML objects to expose to the web page.

    Each object will be added as a property of the web frame's window object.  The
    property name is controlled by the value of \c WebView.windowObjectName
    attached property.

    Exposing QML objects to a web page allows JavaScript executing in the web
    page itself to communicate with QML, by reading and writing properties and
    by calling methods of the exposed QML objects.

    This example shows how to call into a QML method using a window object.

    \qml
    WebView {
        javaScriptWindowObjects: QtObject {
            WebView.windowObjectName: "qml"

            function qmlCall() {
                console.log("This call is in QML!");
            }
        }

        html: "<script>console.log(\"This is in WebKit!\"); window.qml.qmlCall();</script>"
    }
    \endqml

    The output of the example will be:
    \code
    This is in WebKit!
    This call is in QML!
    \endcode

    If Javascript is not enabled for the page, then this property does nothing.
*/
QDeclarativeListProperty<QObject> QDeclarativeWebView::javaScriptWindowObjects()
{
    return QDeclarativeListProperty<QObject>(this, d, &QDeclarativeWebViewPrivate::windowObjectsAppend);
}

QDeclarativeWebViewAttached* QDeclarativeWebView::qmlAttachedProperties(QObject* o)
{
    return new QDeclarativeWebViewAttached(o);
}

void QDeclarativeWebViewPrivate::updateWindowObjects()
{
    if (!q->isComponentCompletePublic() || !q->page())
        return;

    for (int i = 0; i < windowObjects.count(); ++i) {
        QObject* object = windowObjects.at(i);
        QDeclarativeWebViewAttached* attached = static_cast<QDeclarativeWebViewAttached *>(qmlAttachedPropertiesObject<QDeclarativeWebView>(object));
        if (attached && !attached->windowObjectName().isEmpty())
            q->page()->mainFrame()->addToJavaScriptWindowObject(attached->windowObjectName(), object);
    }
}

bool QDeclarativeWebView::renderingEnabled() const
{
    return d->rendering;
}

void QDeclarativeWebView::setRenderingEnabled(bool enabled)
{
    if (d->rendering == enabled)
        return;
    d->rendering = enabled;
    emit renderingEnabledChanged();
    d->view->setTiledBackingStoreFrozen(!enabled);
}

/*!
    \qmlsignal WebView::onDoubleClick(int clickx, int clicky)

    The WebView does not pass double-click events to the web engine, but rather
    emits this signals.
*/

/*!
    \qmlmethod bool WebView::heuristicZoom(int clickX, int clickY, real maxzoom)

    Finds a zoom that:
    \list
    \i shows a whole item
    \i includes (\a clickX, \a clickY)
    \i fits into the preferredWidth and preferredHeight
    \i zooms by no more than \a maxZoom
    \i is more than 10% above the current zoom
    \endlist

    If such a zoom exists, emits zoomTo(zoom,centerX,centerY) and returns true; otherwise,
    no signal is emitted and returns false.
*/
bool QDeclarativeWebView::heuristicZoom(int clickX, int clickY, qreal maxZoom)
{
    if (contentsScale() >= maxZoom / scale())
        return false;
    qreal ozf = contentsScale();
    QRect showArea = elementAreaAt(clickX, clickY, d->preferredwidth / maxZoom, d->preferredheight / maxZoom);
    qreal z = qMin(qreal(d->preferredwidth) / showArea.width(), qreal(d->preferredheight) / showArea.height());
    if (z > maxZoom / scale())
        z = maxZoom / scale();
    if (z / ozf > 1.2) {
        QRectF r(showArea.left() * z, showArea.top() * z, showArea.width() * z, showArea.height() * z);
        emit zoomTo(z, r.x() + r.width() / 2, r.y() + r.height() / 2);
        return true;
    }
    return false;
}

/*!
    \qmlproperty int WebView::pressGrabTime

    The number of milliseconds the user must press before the WebView
    starts passing move events through to the Web engine (rather than
    letting other QML elements such as a Flickable take them).

    Defaults to 400ms. Set to 0 to always grab and pass move events to
    the Web engine.
*/
int QDeclarativeWebView::pressGrabTime() const
{
    return d->view->pressTime;
}

void QDeclarativeWebView::setPressGrabTime(int millis)
{
    if (d->view->pressTime == millis)
        return;
    d->view->pressTime = millis;
    emit pressGrabTimeChanged();
}

#ifndef QT_NO_ACTION
/*!
    \qmlproperty action WebView::back
    This property holds the action for causing the previous URL in the history to be displayed.
*/
QAction* QDeclarativeWebView::backAction() const
{
    return page()->action(QWebPage::Back);
}

/*!
    \qmlproperty action WebView::forward
    This property holds the action for causing the next URL in the history to be displayed.
*/
QAction* QDeclarativeWebView::forwardAction() const
{
    return page()->action(QWebPage::Forward);
}

/*!
    \qmlproperty action WebView::reload
    This property holds the action for reloading with the current URL
*/
QAction* QDeclarativeWebView::reloadAction() const
{
    return page()->action(QWebPage::Reload);
}

/*!
    \qmlproperty action WebView::stop
    This property holds the action for stopping loading with the current URL
*/
QAction* QDeclarativeWebView::stopAction() const
{
    return page()->action(QWebPage::Stop);
}
#endif // QT_NO_ACTION

/*!
    \qmlproperty string WebView::title
    This property holds the title of the web page currently viewed

    By default, this property contains an empty string.
*/
QString QDeclarativeWebView::title() const
{
    return page()->mainFrame()->title();
}

/*!
    \qmlproperty pixmap WebView::icon
    This property holds the icon associated with the web page currently viewed
*/
QPixmap QDeclarativeWebView::icon() const
{
    return page()->mainFrame()->icon().pixmap(QSize(256, 256));
}

/*!
    \qmlproperty string WebView::statusText

    This property is the current status suggested by the current web page. In a web browser,
    such status is often shown in some kind of status bar.
*/
void QDeclarativeWebView::setStatusText(const QString& text)
{
    d->statusText = text;
    emit statusTextChanged();
}

void QDeclarativeWebView::windowObjectCleared()
{
    d->updateWindowObjects();
}

QString QDeclarativeWebView::statusText() const
{
    return d->statusText;
}

QWebPage* QDeclarativeWebView::page() const
{
    return d->view->page();
}

// The QObject interface to settings().
/*!
    \qmlproperty string WebView::settings.standardFontFamily
    \qmlproperty string WebView::settings.fixedFontFamily
    \qmlproperty string WebView::settings.serifFontFamily
    \qmlproperty string WebView::settings.sansSerifFontFamily
    \qmlproperty string WebView::settings.cursiveFontFamily
    \qmlproperty string WebView::settings.fantasyFontFamily

    \qmlproperty int WebView::settings.minimumFontSize
    \qmlproperty int WebView::settings.minimumLogicalFontSize
    \qmlproperty int WebView::settings.defaultFontSize
    \qmlproperty int WebView::settings.defaultFixedFontSize

    \qmlproperty bool WebView::settings.autoLoadImages
    \qmlproperty bool WebView::settings.javascriptEnabled
    \qmlproperty bool WebView::settings.javaEnabled
    \qmlproperty bool WebView::settings.pluginsEnabled
    \qmlproperty bool WebView::settings.privateBrowsingEnabled
    \qmlproperty bool WebView::settings.javascriptCanOpenWindows
    \qmlproperty bool WebView::settings.javascriptCanAccessClipboard
    \qmlproperty bool WebView::settings.developerExtrasEnabled
    \qmlproperty bool WebView::settings.linksIncludedInFocusChain
    \qmlproperty bool WebView::settings.zoomTextOnly
    \qmlproperty bool WebView::settings.printElementBackgrounds
    \qmlproperty bool WebView::settings.offlineStorageDatabaseEnabled
    \qmlproperty bool WebView::settings.offlineWebApplicationCacheEnabled
    \qmlproperty bool WebView::settings.localStorageDatabaseEnabled
    \qmlproperty bool WebView::settings.localContentCanAccessRemoteUrls

    These properties give access to the settings controlling the web view.

    See QWebSettings for details of these properties.

    \qml
        WebView {
            settings.pluginsEnabled: true
            settings.standardFontFamily: "Arial"
            ...
        }
    \endqml
*/
QDeclarativeWebSettings* QDeclarativeWebView::settingsObject() const
{
    d->settings.s = page()->settings();
    return &d->settings;
}

void QDeclarativeWebView::setPage(QWebPage* page)
{
    if (d->view->page() == page)
        return;

    d->view->setPage(page);
    updateContentsSize();
    page->mainFrame()->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);
    page->mainFrame()->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);
    connect(page->mainFrame(), SIGNAL(urlChanged(QUrl)), this, SLOT(pageUrlChanged()));
    connect(page->mainFrame(), SIGNAL(titleChanged(QString)), this, SIGNAL(titleChanged(QString)));
    connect(page->mainFrame(), SIGNAL(titleChanged(QString)), this, SIGNAL(iconChanged()));
    connect(page->mainFrame(), SIGNAL(iconChanged()), this, SIGNAL(iconChanged()));
    connect(page->mainFrame(), SIGNAL(initialLayoutCompleted()), this, SLOT(initialLayout()));
    connect(page->mainFrame(), SIGNAL(contentsSizeChanged(QSize)), this, SIGNAL(contentsSizeChanged(QSize)));

    connect(page, SIGNAL(loadStarted()), this, SLOT(doLoadStarted()));
    connect(page, SIGNAL(loadProgress(int)), this, SLOT(doLoadProgress(int)));
    connect(page, SIGNAL(loadFinished(bool)), this, SLOT(doLoadFinished(bool)));
    connect(page, SIGNAL(statusBarMessage(QString)), this, SLOT(setStatusText(QString)));

    connect(page->mainFrame(), SIGNAL(javaScriptWindowObjectCleared()), this, SLOT(windowObjectCleared()));

    page->settings()->setAttribute(QWebSettings::TiledBackingStoreEnabled, true);

}

/*!
    \qmlsignal WebView::onLoadStarted()

    This handler is called when the web engine begins loading
    a page. Later, WebView::onLoadFinished() or WebView::onLoadFailed()
    will be emitted.
*/

/*!
    \qmlsignal WebView::onLoadFinished()

    This handler is called when the web engine \e successfully
    finishes loading a page, including any component content
    (WebView::onLoadFailed() will be emitted otherwise).

    \sa progress
*/

/*!
    \qmlsignal WebView::onLoadFailed()

    This handler is called when the web engine fails loading
    a page or any component content
    (WebView::onLoadFinished() will be emitted on success).
*/

void QDeclarativeWebView::load(const QNetworkRequest& request, QNetworkAccessManager::Operation operation, const QByteArray& body)
{
    page()->mainFrame()->load(request, operation, body);
}

QString QDeclarativeWebView::html() const
{
    return page()->mainFrame()->toHtml();
}

/*!
    \qmlproperty string WebView::html
    This property holds HTML text set directly

    The html property can be set as a string.

    \qml
    WebView {
        html: "<p>This is <b>HTML</b>."
    }
    \endqml
*/
void QDeclarativeWebView::setHtml(const QString& html, const QUrl& baseUrl)
{
    updateContentsSize();
    if (isComponentComplete())
        page()->mainFrame()->setHtml(html, baseUrl);
    else {
        d->pending = d->PendingHtml;
        d->pendingUrl = baseUrl;
        d->pendingString = html;
    }
    emit htmlChanged();
}

void QDeclarativeWebView::setContent(const QByteArray& data, const QString& mimeType, const QUrl& baseUrl)
{
    updateContentsSize();

    if (isComponentComplete())
        page()->mainFrame()->setContent(data, mimeType, qmlContext(this)->resolvedUrl(baseUrl));
    else {
        d->pending = d->PendingContent;
        d->pendingUrl = baseUrl;
        d->pendingString = mimeType;
        d->pendingData = data;
    }
}

QWebHistory* QDeclarativeWebView::history() const
{
    return page()->history();
}

QWebSettings* QDeclarativeWebView::settings() const
{
    return page()->settings();
}

QDeclarativeWebView* QDeclarativeWebView::createWindow(QWebPage::WebWindowType type)
{
    switch (type) {
    case QWebPage::WebBrowserWindow: {
        if (!d->newWindowComponent && d->newWindowParent)
            qWarning("WebView::newWindowComponent not set - WebView::newWindowParent ignored");
        else if (d->newWindowComponent && !d->newWindowParent)
            qWarning("WebView::newWindowParent not set - WebView::newWindowComponent ignored");
        else if (d->newWindowComponent && d->newWindowParent) {
            QDeclarativeWebView* webview = 0;
            QDeclarativeContext* windowContext = new QDeclarativeContext(qmlContext(this));

            QObject* newObject = d->newWindowComponent->create(windowContext);
            if (newObject) {
                windowContext->setParent(newObject);
                QDeclarativeItem* item = qobject_cast<QDeclarativeItem *>(newObject);
                if (!item)
                    delete newObject;
                else {
                    webview = item->findChild<QDeclarativeWebView*>();
                    if (!webview)
                        delete item;
                    else {
                        newObject->setParent(d->newWindowParent);
                        static_cast<QGraphicsObject*>(item)->setParentItem(d->newWindowParent);
                    }
                }
            } else
                delete windowContext;

            return webview;
        }
    }
    break;
    case QWebPage::WebModalDialog: {
        // Not supported
    }
    }
    return 0;
}

/*!
    \qmlproperty component WebView::newWindowComponent

    This property holds the component to use for new windows.
    The component must have a WebView somewhere in its structure.

    When the web engine requests a new window, it will be an instance of
    this component.

    The parent of the new window is set by newWindowParent. It must be set.
*/
QDeclarativeComponent* QDeclarativeWebView::newWindowComponent() const
{
    return d->newWindowComponent;
}

void QDeclarativeWebView::setNewWindowComponent(QDeclarativeComponent* newWindow)
{
    if (newWindow == d->newWindowComponent)
        return;
    d->newWindowComponent = newWindow;
    emit newWindowComponentChanged();
}


/*!
    \qmlproperty item WebView::newWindowParent

    The parent item for new windows.

    \sa newWindowComponent
*/
QDeclarativeItem* QDeclarativeWebView::newWindowParent() const
{
    return d->newWindowParent;
}

void QDeclarativeWebView::setNewWindowParent(QDeclarativeItem* parent)
{
    if (parent == d->newWindowParent)
        return;
    if (d->newWindowParent && parent) {
        QList<QGraphicsItem *> children = d->newWindowParent->childItems();
        for (int i = 0; i < children.count(); ++i)
            children.at(i)->setParentItem(parent);
    }
    d->newWindowParent = parent;
    emit newWindowParentChanged();
}

QSize QDeclarativeWebView::contentsSize() const
{
    return page()->mainFrame()->contentsSize() * contentsScale();
}

qreal QDeclarativeWebView::contentsScale() const
{
    return d->view->scale();
}

void QDeclarativeWebView::setContentsScale(qreal scale)
{
    if (scale == d->view->scale())
        return;
    d->view->setScale(scale);
    updateDeclarativeWebViewSize();
    emit contentsScaleChanged();
}

/*!
    Returns the area of the largest element at position (\a x,\a y) that is no larger
    than \a maxWidth by \a maxHeight pixels.

    May return an area larger in the case when no smaller element is at the position.
*/
QRect QDeclarativeWebView::elementAreaAt(int x, int y, int maxWidth, int maxHeight) const
{
    QWebHitTestResult hit = page()->mainFrame()->hitTestContent(QPoint(x, y));
    QRect hitRect = hit.boundingRect();
    QWebElement element = hit.enclosingBlockElement();
    if (maxWidth <= 0)
        maxWidth = INT_MAX;
    if (maxHeight <= 0)
        maxHeight = INT_MAX;
    while (!element.parent().isNull() && element.geometry().width() <= maxWidth && element.geometry().height() <= maxHeight) {
        hitRect = element.geometry();
        element = element.parent();
    }
    return hitRect;
}

/*!
    \internal
    \class QDeclarativeWebPage
    \brief The QDeclarativeWebPage class is a QWebPage that can create QML plugins.

    \sa QDeclarativeWebView
*/
QDeclarativeWebPage::QDeclarativeWebPage(QDeclarativeWebView* parent) :
    QWebPage(parent)
{
}

QDeclarativeWebPage::~QDeclarativeWebPage()
{
}

QString QDeclarativeWebPage::chooseFile(QWebFrame* originatingFrame, const QString& oldFile)
{
    // Not supported (it's modal)
    Q_UNUSED(originatingFrame)
    Q_UNUSED(oldFile)
    return oldFile;
}

/*!
    \qmlsignal WebView::onAlert(string message)

    The handler is called when the web engine sends a JavaScript alert. The \a message is the text
    to be displayed in the alert to the user.
*/


void QDeclarativeWebPage::javaScriptAlert(QWebFrame* originatingFrame, const QString& msg)
{
    Q_UNUSED(originatingFrame)
    emit viewItem()->alert(msg);
}

bool QDeclarativeWebPage::javaScriptConfirm(QWebFrame* originatingFrame, const QString& msg)
{
    // Not supported (it's modal)
    Q_UNUSED(originatingFrame)
    Q_UNUSED(msg)
    return false;
}

bool QDeclarativeWebPage::javaScriptPrompt(QWebFrame* originatingFrame, const QString& msg, const QString& defaultValue, QString* result)
{
    // Not supported (it's modal)
    Q_UNUSED(originatingFrame)
    Q_UNUSED(msg)
    Q_UNUSED(defaultValue)
    Q_UNUSED(result)
    return false;
}


QDeclarativeWebView* QDeclarativeWebPage::viewItem()
{
    return static_cast<QDeclarativeWebView*>(parent());
}

QWebPage* QDeclarativeWebPage::createWindow(WebWindowType type)
{
    QDeclarativeWebView* newView = viewItem()->createWindow(type);
    if (newView)
        return newView->page();
    return 0;
}

QT_END_NAMESPACE

