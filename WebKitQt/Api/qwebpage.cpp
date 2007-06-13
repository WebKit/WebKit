/*
    Copyright (C) 2007 Trolltech ASA
    Copyright (C) 2007 Staikos Computing Services Inc.
    Copyright (C) 2007 Apple Inc.

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "qwebpage.h"
#include "qwebframe.h"
#include "qwebpage_p.h"
#include "qwebframe_p.h"
#include "qwebnetworkinterface.h"
#include "qwebpagehistory.h"
#include "qwebpagehistory_p.h"
#include "qwebsettings.h"

#include "Frame.h"
#include "ChromeClientQt.h"
#include "ContextMenuClientQt.h"
#include "DragClientQt.h"
#include "DragController.h"
#include "DragData.h"
#include "EditorClientQt.h"
#include "Settings.h"
#include "Page.h"
#include "FrameLoader.h"
#include "KURL.h"

#include <QDebug>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QUndoStack>
#include <QUrl>
#include <QVBoxLayout>
#include <QHttpRequestHeader>

using namespace WebCore;

QWebPagePrivate::QWebPagePrivate(QWebPage *qq)
    : q(qq), modified(false)
{
    chromeClient = new ChromeClientQt(q);
    contextMenuClient = new ContextMenuClientQt();
    editorClient = new EditorClientQt(q);
    page = new Page(chromeClient, contextMenuClient, editorClient,
                    new DragClientQt(q));

    undoStack = 0;
    mainFrame = 0;
    networkInterface = 0;
    insideOpenCall = false;
}

QWebPagePrivate::~QWebPagePrivate()
{
    delete undoStack;
    delete page;
}

QWebPage::NavigationRequestResponse QWebPagePrivate::navigationRequested(QWebFrame *frame, const QUrl &url, const QHttpRequestHeader &request, const QByteArray &postData)
{
    if (insideOpenCall
        && frame == mainFrame)
        return QWebPage::AcceptNavigationRequest;
    return q->navigationRequested(frame, url, request, postData);
}

void QWebPagePrivate::createMainFrame()
{
    if (!mainFrame) {
        QWebFrameData frameData;
        frameData.ownerElement = 0;
        frameData.allowsScrolling = true;
        frameData.marginWidth = 0;
        frameData.marginHeight = 0;
        mainFrame = q->createFrame(0, &frameData);
        layout->addWidget(mainFrame);
    }
}


QWebPage::QWebPage(QWidget *parent)
    : QWidget(parent)
    , d(new QWebPagePrivate(this))
{
    setSettings(QWebSettings::global());
    d->layout = new QVBoxLayout(this);
    d->layout->setMargin(0);
    d->layout->setSpacing(0);
    setAcceptDrops(true);
}

QWebPage::~QWebPage()
{
    FrameLoader *loader = d->mainFrame->d->frame->loader();
    if (loader)
        loader->detachFromParent();
    delete d;
}

QWebFrame *QWebPage::createFrame(QWebFrame *parentFrame, QWebFrameData *frameData)
{
    if (parentFrame)
        return new QWebFrame(parentFrame, frameData);
    QWebFrame *f = new QWebFrame(this, frameData);
    connect(f, SIGNAL(titleChanged(const QString&)),
            SIGNAL(titleChanged(const QString&)));
    connect(f, SIGNAL(hoveringOverLink(const QString&, const QString&)),
            SIGNAL(hoveringOverLink(const QString&, const QString&)));
    return f;
}

void QWebPage::open(const QUrl &url)
{
    open(url, QHttpRequestHeader(), QByteArray());
}

void QWebPage::open(const QUrl &url, const QHttpRequestHeader &httpHeader, const QByteArray &postData)
{
    d->insideOpenCall = true;
    WebCore::ResourceRequest request(KURL(url.toString()));

    if (httpHeader.isValid()) {
        request.setHTTPMethod(httpHeader.method());
        foreach (QString key, httpHeader.keys()) {
            request.addHTTPHeaderField(key, httpHeader.value(key));
        }

        if (!postData.isEmpty()) {
            WTF::RefPtr<WebCore::FormData> formData = new WebCore::FormData(postData.constData(), postData.size());
            request.setHTTPBody(formData);
        }
    }

    mainFrame()->d->frame->loader()->load(request);
    d->insideOpenCall = false;
}

QUrl QWebPage::url() const
{
    return QUrl((QString)mainFrame()->d->frame->loader()->url().url());
}

QString QWebPage::title() const
{
    return mainFrame()->title();
}

QWebFrame *QWebPage::mainFrame() const
{
    d->createMainFrame();
    return d->mainFrame;
}

QSize QWebPage::sizeHint() const
{
    return QSize(800, 600);
}

void QWebPage::stop()
{
    FrameLoader *f = mainFrame()->d->frame->loader();
    f->stopForUserCancel();
}

QWebPageHistory QWebPage::history() const
{
    WebCore::BackForwardList *lst = d->page->backForwardList();
    QWebPageHistoryPrivate *priv = new QWebPageHistoryPrivate(lst);
    return QWebPageHistory(priv);
}

void QWebPage::goBack()
{
    d->page->goBack();
}

void QWebPage::goForward()
{
    d->page->goForward();
}

void QWebPage::goToHistoryItem(const QWebHistoryItem &item)
{
    d->page->goToItem(item.d->item, FrameLoadTypeIndexedBackForward);
}

void QWebPage::javaScriptConsoleMessage(const QString& message, unsigned int lineNumber, const QString& sourceID)
{
}

void QWebPage::statusTextChanged(const QString& text)
{
}

void QWebPage::runJavaScriptAlert(QWebFrame *frame, const QString& msg)
{
}

QWebPage *QWebPage::createWindow()
{
    return 0;
}

QWebPage::NavigationRequestResponse QWebPage::navigationRequested(QWebFrame *frame, const QUrl &url, const QHttpRequestHeader &request, const QByteArray &postData)
{
    Q_UNUSED(frame)
    Q_UNUSED(url)
    Q_UNUSED(request)
    Q_UNUSED(postData)
    return AcceptNavigationRequest;
}

void QWebPage::setWindowGeometry(const QRect& geom)
{
    Q_UNUSED(geom)
}

/*!
  Returns true if the page contains unsubmitted form data.
*/
bool QWebPage::isModified() const
{
    return d->modified;
}


QUndoStack *QWebPage::undoStack()
{
    if (!d->undoStack)
        d->undoStack = new QUndoStack(this);

    return d->undoStack;
}

static inline DragOperation dropActionToDragOp(Qt::DropActions actions)
{
    unsigned result = 0;
    if (actions & Qt::CopyAction)
        result |= DragOperationCopy;
    if (actions & Qt::MoveAction)
        result |= DragOperationMove;
    if (actions & Qt::LinkAction)
        result |= DragOperationLink;
    return (DragOperation)result;    
}

static inline Qt::DropAction dragOpToDropAction(unsigned actions)
{
    Qt::DropAction result = Qt::IgnoreAction;
    if (actions & DragOperationCopy)
        result = Qt::CopyAction;
    else if (actions & DragOperationMove)
        result = Qt::MoveAction;
    else if (actions & DragOperationLink)
        result = Qt::LinkAction;
    return result;    
}

void QWebPage::dragEnterEvent(QDragEnterEvent *ev)
{
    DragData dragData(ev->mimeData(), ev->pos(), QCursor::pos(), 
                      dropActionToDragOp(ev->possibleActions()));
    Qt::DropAction action = dragOpToDropAction(d->page->dragController()->dragEntered(&dragData));
    ev->setDropAction(action);
    ev->accept();
}

void QWebPage::dragLeaveEvent(QDragLeaveEvent *ev)
{
    DragData dragData(0, IntPoint(), QCursor::pos(), DragOperationNone);
    d->page->dragController()->dragExited(&dragData);
    ev->accept();
}

void QWebPage::dragMoveEvent(QDragMoveEvent *ev)
{
    DragData dragData(ev->mimeData(), ev->pos(), QCursor::pos(), 
                      dropActionToDragOp(ev->possibleActions()));
    Qt::DropAction action = dragOpToDropAction(d->page->dragController()->dragUpdated(&dragData));
    ev->setDropAction(action);
    ev->accept();
}

void QWebPage::dropEvent(QDropEvent *ev)
{
    DragData dragData(ev->mimeData(), ev->pos(), QCursor::pos(), 
                      dropActionToDragOp(ev->possibleActions()));
    Qt::DropAction action = dragOpToDropAction(d->page->dragController()->performDrag(&dragData));
    ev->accept();
}

void QWebPage::setNetworkInterface(QWebNetworkInterface *interface)
{
    d->networkInterface = interface;
}

QWebNetworkInterface *QWebPage::networkInterface() const
{
    if (d->networkInterface)
        return d->networkInterface;
    else
        return QWebNetworkInterface::defaultInterface();
}

void QWebPage::setSettings(const QWebSettings &settings)
{
    WebCore::Settings *wSettings = d->page->settings();

    wSettings->setStandardFontFamily(
        settings.fontFamily(QWebSettings::StandardFont));
    wSettings->setFixedFontFamily(
        settings.fontFamily(QWebSettings::FixedFont));
    wSettings->setSerifFontFamily(
        settings.fontFamily(QWebSettings::SerifFont));
    wSettings->setSansSerifFontFamily(
        settings.fontFamily(QWebSettings::SansSerifFont));
    wSettings->setCursiveFontFamily(
        settings.fontFamily(QWebSettings::CursiveFont));
    wSettings->setFantasyFontFamily(
        settings.fontFamily(QWebSettings::FantasyFont));

    wSettings->setMinimumFontSize(settings.minimumFontSize());
    wSettings->setMinimumLogicalFontSize(settings.minimumLogicalFontSize());
    wSettings->setDefaultFontSize(settings.defaultFontSize());
    wSettings->setDefaultFixedFontSize(settings.defaultFixedFontSize());

    wSettings->setLoadsImagesAutomatically(
        settings.testAttribute(QWebSettings::AutoLoadImages));
    wSettings->setJavaScriptEnabled(
        settings.testAttribute(QWebSettings::JavascriptEnabled));
    wSettings->setJavaScriptCanOpenWindowsAutomatically(
        settings.testAttribute(QWebSettings::JavascriptCanOpenWindows));
    wSettings->setJavaEnabled(
        settings.testAttribute(QWebSettings::JavaEnabled));
    wSettings->setPluginsEnabled(
        settings.testAttribute(QWebSettings::PluginsEnabled));
    wSettings->setPrivateBrowsingEnabled(
        settings.testAttribute(QWebSettings::PrivateBrowsingEnabled));

    wSettings->setUserStyleSheetLocation(KURL(settings.userStyleSheetLocation()));
}

QWebSettings QWebPage::settings() const
{
    QWebSettings settings;
    WebCore::Settings *wSettings = d->page->settings();

    settings.setFontFamily(QWebSettings::StandardFont,
                           wSettings->standardFontFamily());
    settings.setFontFamily(QWebSettings::FixedFont,
                           wSettings->fixedFontFamily());
    settings.setFontFamily(QWebSettings::SerifFont,
                           wSettings->serifFontFamily());
    settings.setFontFamily(QWebSettings::SansSerifFont,
                           wSettings->sansSerifFontFamily());
    settings.setFontFamily(QWebSettings::CursiveFont,
                           wSettings->cursiveFontFamily());
    settings.setFontFamily(QWebSettings::FantasyFont,
                           wSettings->fantasyFontFamily());

    settings.setMinimumFontSize(wSettings->minimumFontSize());
    settings.setMinimumLogicalFontSize(wSettings->minimumLogicalFontSize());
    settings.setDefaultFontSize(wSettings->defaultFontSize());
    settings.setDefaultFixedFontSize(wSettings->defaultFixedFontSize());

    settings.setAttribute(QWebSettings::AutoLoadImages,
                          wSettings->loadsImagesAutomatically());
    settings.setAttribute(QWebSettings::JavascriptEnabled,
                          wSettings->isJavaScriptEnabled());
    settings.setAttribute(QWebSettings::JavascriptCanOpenWindows,
                          wSettings->JavaScriptCanOpenWindowsAutomatically());
    settings.setAttribute(QWebSettings::JavaEnabled,
                          wSettings->isJavaEnabled());
    settings.setAttribute(QWebSettings::PluginsEnabled,
                          wSettings->arePluginsEnabled());
    settings.setAttribute(QWebSettings::PrivateBrowsingEnabled,
                          wSettings->privateBrowsingEnabled());

    settings.setUserStyleSheetLocation(
        wSettings->userStyleSheetLocation().url());

    return settings;
}
