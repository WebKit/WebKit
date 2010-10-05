/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "qwkpage.h"
#include "qwkpage_p.h"

#include "qwkpreferences_p.h"

#include "ClientImpl.h"
#include "LocalizedStrings.h"
#include "NativeWebKeyboardEvent.h"
#include "WebContext.h"
#include "WebEventFactoryQt.h"
#include "WebPlatformStrategies.h"
#include "WKStringQt.h"
#include "WKURLQt.h"
#include "ViewportArguments.h"
#include <QAction>
#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QStyle>
#include <QTouchEvent>
#include <QtDebug>
#include <WebKit2/WKBase.h>
#include <WebKit2/WKFrame.h>
#include <WebKit2/WKPage.h>
#include <WebKit2/WKRetainPtr.h>

using namespace WebKit;
using namespace WebCore;

QWKPagePrivate::QWKPagePrivate(QWKPage* qq, WKPageNamespaceRef namespaceRef)
    : q(qq)
    , preferences(0)
    , createNewPageFn(0)
{
    // We want to use the LocalizationStrategy at the UI side as well.
    // FIXME: this should be avoided.
    WebPlatformStrategies::initialize();

    memset(actions, 0, sizeof(actions));
    page = toWK(namespaceRef)->createWebPage();
    page->setPageClient(this);
    pageNamespaceRef = namespaceRef;
}

QWKPagePrivate::~QWKPagePrivate()
{
    page->close();
}

void QWKPagePrivate::init(const QSize& viewportSize, PassOwnPtr<DrawingAreaProxy> proxy)
{
    page->setDrawingArea(proxy);
    page->initializeWebPage(IntSize(viewportSize));
}

void QWKPagePrivate::setCursor(const WebCore::Cursor& cursor)
{
#ifndef QT_NO_CURSOR
    emit q->cursorChanged(*cursor.platformCursor());
#endif
}

void QWKPagePrivate::toolTipChanged(const String&, const String& newTooltip)
{
    emit q->statusBarMessage(QString(newTooltip));
}

void QWKPagePrivate::registerEditCommand(PassRefPtr<WebEditCommandProxy>, UndoOrRedo)
{
}

void QWKPagePrivate::clearAllEditCommands()
{
}

void QWKPagePrivate::paint(QPainter* painter, QRect area)
{
    painter->save();

    painter->setBrush(Qt::white);
    painter->drawRect(area);

    if (page->isValid() && page->drawingArea())
        page->drawingArea()->paint(IntRect(area), painter);

    painter->restore();
}

void QWKPagePrivate::keyPressEvent(QKeyEvent* ev)
{
    page->handleKeyboardEvent(NativeWebKeyboardEvent(ev));
}

void QWKPagePrivate::keyReleaseEvent(QKeyEvent* ev)
{
    page->handleKeyboardEvent(NativeWebKeyboardEvent(ev));
}

void QWKPagePrivate::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
    // For some reason mouse press results in mouse hover (which is
    // converted to mouse move for WebKit). We ignore these hover
    // events by comparing lastPos with newPos.
    // NOTE: lastPos from the event always comes empty, so we work
    // around that here.
    static QPointF lastPos = QPointF();
    if (lastPos == ev->pos())
        return;
    lastPos = ev->pos();

    WebMouseEvent mouseEvent = WebEventFactory::createWebMouseEvent(ev, 0);
    page->handleMouseEvent(mouseEvent);
}

void QWKPagePrivate::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
    if (tripleClickTimer.isActive() && (ev->pos() - tripleClick).manhattanLength() < QApplication::startDragDistance()) {
        WebMouseEvent mouseEvent = WebEventFactory::createWebMouseEvent(ev, 3);
        page->handleMouseEvent(mouseEvent);
        return;
    }

    WebMouseEvent mouseEvent = WebEventFactory::createWebMouseEvent(ev, 1);
    page->handleMouseEvent(mouseEvent);
}

void QWKPagePrivate::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
    WebMouseEvent mouseEvent = WebEventFactory::createWebMouseEvent(ev, 0);
    page->handleMouseEvent(mouseEvent);
}

void QWKPagePrivate::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* ev)
{
    WebMouseEvent mouseEvent = WebEventFactory::createWebMouseEvent(ev, 2);
    page->handleMouseEvent(mouseEvent);

    tripleClickTimer.start(QApplication::doubleClickInterval(), q);
    tripleClick = ev->pos().toPoint();
}

void QWKPagePrivate::wheelEvent(QGraphicsSceneWheelEvent* ev)
{
    WebWheelEvent wheelEvent = WebEventFactory::createWebWheelEvent(ev);
    page->handleWheelEvent(wheelEvent);
}

void QWKPagePrivate::setEditCommandState(const WTF::String&, bool, int)
{
}

void QWKPagePrivate::updateAction(QWKPage::WebAction action)
{
#ifdef QT_NO_ACTION
    Q_UNUSED(action)
#else
    QAction* a = actions[action];
    if (!a)
        return;

    RefPtr<WebKit::WebFrameProxy> mainFrame = page->mainFrame();
    if (!mainFrame)
        return;

    bool enabled = a->isEnabled();
    bool checked = a->isChecked();

    switch (action) {
    case QWKPage::Back:
        enabled = page->canGoBack();
        break;
    case QWKPage::Forward:
        enabled = page->canGoForward();
        break;
    case QWKPage::Stop:
        enabled = !(WebFrameProxy::LoadStateFinished == mainFrame->loadState());
        break;
    case QWKPage::Reload:
        enabled = (WebFrameProxy::LoadStateFinished == mainFrame->loadState());
        break;
    default:
        break;
    }

    a->setEnabled(enabled);

    if (a->isCheckable())
        a->setChecked(checked);
#endif // QT_NO_ACTION
}

void QWKPagePrivate::updateNavigationActions()
{
    updateAction(QWKPage::Back);
    updateAction(QWKPage::Forward);
    updateAction(QWKPage::Stop);
    updateAction(QWKPage::Reload);
}

#ifndef QT_NO_ACTION
void QWKPagePrivate::_q_webActionTriggered(bool checked)
{
    QAction* a = qobject_cast<QAction*>(q->sender());
    if (!a)
        return;
    QWKPage::WebAction action = static_cast<QWKPage::WebAction>(a->data().toInt());
    q->triggerAction(action, checked);
}
#endif // QT_NO_ACTION

void QWKPagePrivate::touchEvent(QTouchEvent* event)
{
    WebTouchEvent touchEvent = WebEventFactory::createWebTouchEvent(event);
    page->handleTouchEvent(touchEvent);
}

QWKPage::QWKPage(WKPageNamespaceRef namespaceRef)
    : d(new QWKPagePrivate(this, namespaceRef))
{
    WKPageLoaderClient loadClient = {
        0,      /* version */
        this,   /* clientInfo */
        qt_wk_didStartProvisionalLoadForFrame,
        qt_wk_didReceiveServerRedirectForProvisionalLoadForFrame,
        qt_wk_didFailProvisionalLoadWithErrorForFrame,
        qt_wk_didCommitLoadForFrame,
        qt_wk_didFinishDocumentLoadForFrame,
        qt_wk_didFinishLoadForFrame,
        qt_wk_didFailLoadWithErrorForFrame,
        qt_wk_didReceiveTitleForFrame,
        qt_wk_didFirstLayoutForFrame,
        qt_wk_didFirstVisuallyNonEmptyLayoutForFrame,
        qt_wk_didRemoveFrameFromHierarchy,
        qt_wk_didStartProgress,
        qt_wk_didChangeProgress,
        qt_wk_didFinishProgress,
        qt_wk_didBecomeUnresponsive,
        qt_wk_didBecomeResponsive,
        0,  /* processDidExit */
        0   /* didChangeBackForwardList */
    };
    WKPageSetPageLoaderClient(pageRef(), &loadClient);

    WKPageUIClient uiClient = {
        0,      /* version */
        this,   /* clientInfo */
        qt_wk_createNewPage,
        qt_wk_showPage,
        qt_wk_close,
        qt_wk_runJavaScriptAlert,
        0,  /* runJavaScriptConfirm */
        0,  /* runJavaScriptPrompt */
        0,  /* setStatusText */
        0,  /* mouseDidMoveOverElement */
        0,  /* contentsSizeChanged */
        0   /* didNotHandleKeyEvent */
    };
    WKPageSetPageUIClient(pageRef(), &uiClient);
}

QWKPage::~QWKPage()
{
    delete d;
}

QWKPage::ViewportConfiguration::ViewportConfiguration()
    : d(0)
    , m_initialScaleFactor(-1.0)
    , m_minimumScaleFactor(-1.0)
    , m_maximumScaleFactor(-1.0)
    , m_devicePixelRatio(-1.0)
    , m_isUserScalable(true)
    , m_isValid(false)
{

}

QWKPage::ViewportConfiguration::ViewportConfiguration(const QWKPage::ViewportConfiguration& other)
    : d(other.d)
    , m_initialScaleFactor(other.m_initialScaleFactor)
    , m_minimumScaleFactor(other.m_minimumScaleFactor)
    , m_maximumScaleFactor(other.m_maximumScaleFactor)
    , m_devicePixelRatio(other.m_devicePixelRatio)
    , m_isUserScalable(other.m_isUserScalable)
    , m_isValid(other.m_isValid)
    , m_size(other.m_size)
{

}

QWKPage::ViewportConfiguration::~ViewportConfiguration()
{

}

QWKPage::ViewportConfiguration& QWKPage::ViewportConfiguration::operator=(const QWKPage::ViewportConfiguration& other)
{
    if (this != &other) {
        d = other.d;
        m_initialScaleFactor = other.m_initialScaleFactor;
        m_minimumScaleFactor = other.m_minimumScaleFactor;
        m_maximumScaleFactor = other.m_maximumScaleFactor;
        m_devicePixelRatio = other.m_devicePixelRatio;
        m_isUserScalable = other.m_isUserScalable;
        m_isValid = other.m_isValid;
        m_size = other.m_size;
    }

    return *this;
}

QWKPage::ViewportConfiguration QWKPage::viewportConfigurationForSize(QSize availableSize) const
{
    static int desktopWidth = 980;
    static int deviceDPI = 160;

    // FIXME: Add a way to get these data via the platform plugin and fall back
    // to the size of the view.
    int deviceWidth = 480;
    int deviceHeight = 864;

    ViewportArguments args;

    WebCore::ViewportConfiguration conf = WebCore::findConfigurationForViewportData(args, desktopWidth, deviceWidth, deviceHeight, deviceDPI, availableSize);

    ViewportConfiguration result;

    result.m_isValid = true;
    result.m_size = conf.layoutViewport;
    result.m_initialScaleFactor = conf.initialScale;
    result.m_minimumScaleFactor = conf.minimumScale;
    result.m_maximumScaleFactor = conf.maximumScale;
    result.m_devicePixelRatio = conf.devicePixelRatio;

    return result;
}

void QWKPage::timerEvent(QTimerEvent* ev)
{
    int timerId = ev->timerId();
    if (timerId == d->tripleClickTimer.timerId())
        d->tripleClickTimer.stop();
    else
        QObject::timerEvent(ev);
}

WKPageRef QWKPage::pageRef() const
{
    return toRef(d->page.get());
}

QWKPreferences* QWKPage::preferences() const
{
    if (!d->preferences) {
        WKContextRef contextRef = WKPageNamespaceGetContext(d->pageNamespaceRef);
        d->preferences = QWKPreferencesPrivate::createPreferences(contextRef);
    }

    return d->preferences;
}

void QWKPage::setCreateNewPageFunction(CreateNewPageFn function)
{
    d->createNewPageFn = function;
}

void QWKPage::load(const QUrl& url)
{
    WKRetainPtr<WKURLRef> wkurl(WKURLCreateWithQUrl(url));
    WKPageLoadURL(pageRef(), wkurl.get());
}

void QWKPage::setUrl(const QUrl& url)
{
    load(url);
}

QUrl QWKPage::url() const
{
    WKRetainPtr<WKFrameRef> frame = WKPageGetMainFrame(pageRef());
    if (!frame)
        return QUrl();
    return WKURLCopyQUrl(WKFrameCopyURL(frame.get()));
}

QString QWKPage::title() const
{
    return WKStringCopyQString(WKPageCopyTitle(pageRef()));
}

void QWKPage::setViewportSize(const QSize& size)
{
    if (d->page->drawingArea())
        d->page->drawingArea()->setSize(IntSize(size));
}

#ifndef QT_NO_ACTION
void QWKPage::triggerAction(WebAction action, bool)
{
    switch (action) {
    case Back:
        d->page->goBack();
        break;
    case Forward:
        d->page->goForward();
        break;
    case Stop:
        d->page->stopLoading();
        break;
    case Reload:
        d->page->reload(/* reloadFromOrigin */ true);
        break;
    default:
        break;
    }
}
#endif // QT_NO_ACTION

#ifndef QT_NO_ACTION
QAction* QWKPage::action(WebAction action) const
{
    if (action == QWKPage::NoWebAction || action >= WebActionCount)
        return 0;

    if (d->actions[action])
        return d->actions[action];

    QString text;
    QIcon icon;
    QStyle* style = qobject_cast<QApplication*>(QCoreApplication::instance())->style();
    bool checkable = false;

    switch (action) {
    case Back:
        text = contextMenuItemTagGoBack();
        icon = style->standardIcon(QStyle::SP_ArrowBack);
        break;
    case Forward:
        text = contextMenuItemTagGoForward();
        icon = style->standardIcon(QStyle::SP_ArrowForward);
        break;
    case Stop:
        text = contextMenuItemTagStop();
        icon = style->standardIcon(QStyle::SP_BrowserStop);
        break;
    case Reload:
        text = contextMenuItemTagReload();
        icon = style->standardIcon(QStyle::SP_BrowserReload);
        break;
    default:
        return 0;
        break;
    }

    if (text.isEmpty())
        return 0;

    QAction* a = new QAction(d->q);
    a->setText(text);
    a->setData(action);
    a->setCheckable(checkable);
    a->setIcon(icon);

    connect(a, SIGNAL(triggered(bool)), this, SLOT(_q_webActionTriggered(bool)));

    d->actions[action] = a;
    d->updateAction(action);
    return a;
}
#endif // QT_NO_ACTION

#include "moc_qwkpage.cpp"
