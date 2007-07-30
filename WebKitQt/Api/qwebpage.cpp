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
#include "Image.h"
#include "IconDatabase.h"
#include "InspectorClientQt.h"
#include "FocusController.h"
#include "Editor.h"
#include "PlatformScrollBar.h"
#include "PlatformKeyboardEvent.h"

#include <QDebug>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QHttpRequestHeader>
#include <QInputDialog>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QUndoStack>
#include <QUrl>
#include <QPainter>

using namespace WebCore;

QWebPagePrivate::QWebPagePrivate(QWebPage *qq)
    : q(qq), modified(false)
{
    q->setMouseTracking(true);
    q->setFocusPolicy(Qt::ClickFocus);
    chromeClient = new ChromeClientQt(q);
    contextMenuClient = new ContextMenuClientQt();
    editorClient = new EditorClientQt(q);
    page = new Page(chromeClient, contextMenuClient, editorClient,
                    new DragClientQt(q), new InspectorClientQt());

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

QWebPage::NavigationRequestResponse QWebPagePrivate::navigationRequested(QWebFrame *frame, const QWebNetworkRequest &request, QWebPage::NavigationType type)
{
    if (insideOpenCall
        && frame == mainFrame)
        return QWebPage::AcceptNavigationRequest;
    return q->navigationRequested(frame, request, type);
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
        mainFrame->d->frameView->setFrameGeometry(q->geometry());
    }
}


QWebPage::QWebPage(QWidget *parent)
    : QWidget(parent)
    , d(new QWebPagePrivate(this))
{
    setSettings(QWebSettings::global());
    QPalette pal = palette();
    pal.setBrush(QPalette::Background, Qt::white);

    setAttribute(Qt::WA_OpaquePaintEvent);

    setPalette(pal);
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
    open(QWebNetworkRequest(url));
}

void QWebPage::open(const QWebNetworkRequest &req)
{
    d->insideOpenCall = true;

    QUrl url = req.url();
    QHttpRequestHeader httpHeader = req.httpHeader();
    QByteArray postData = req.postData();

    WebCore::ResourceRequest request(KURL(url.toString()));

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

void QWebPage::javaScriptAlert(QWebFrame *frame, const QString& msg)
{
    //FIXME frame pos...
    QMessageBox::information(this, title(), msg, QMessageBox::Ok);
}

bool QWebPage::javaScriptConfirm(QWebFrame *frame, const QString& msg)
{
    //FIXME frame pos...
    return 0 == QMessageBox::information(this, title(), msg, QMessageBox::Yes, QMessageBox::No);
}

bool QWebPage::javaScriptPrompt(QWebFrame *frame, const QString& msg, const QString& defaultValue, QString* result)
{
    //FIXME frame pos...
    bool ok = false;
#ifndef QT_NO_INPUTDIALOG
    QString x = QInputDialog::getText(this, title(), msg, QLineEdit::Normal, defaultValue, &ok);
    if (ok && result) {
        *result = x;
    }
#endif
    return ok;
}

QWebPage *QWebPage::createWindow()
{
    return 0;
}

QWebPage::NavigationRequestResponse QWebPage::navigationRequested(QWebFrame *frame, const QWebNetworkRequest &request, QWebPage::NavigationType type)
{
    Q_UNUSED(request)
    return AcceptNavigationRequest;
}

void QWebPage::setWindowGeometry(const QRect& geom)
{
    Q_UNUSED(geom)
}

bool QWebPage::canCut() const
{
    return d->page->focusController()->focusedOrMainFrame()->editor()->canCut();
}

bool QWebPage::canCopy() const
{
    return d->page->focusController()->focusedOrMainFrame()->editor()->canCopy();
}

bool QWebPage::canPaste() const
{
    return d->page->focusController()->focusedOrMainFrame()->editor()->canPaste();
}

void QWebPage::cut()
{
    d->page->focusController()->focusedOrMainFrame()->editor()->cut();
}

void QWebPage::copy()
{
    d->page->focusController()->focusedOrMainFrame()->editor()->copy();
}

void QWebPage::paste()
{
    d->page->focusController()->focusedOrMainFrame()->editor()->paste();
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

void QWebPage::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (mainFrame()->d->frame && mainFrame()->d->frameView) {
        mainFrame()->d->frameView->setFrameGeometry(rect());
        mainFrame()->d->frame->forceLayout();
        mainFrame()->d->frame->view()->adjustViewSize();
    }
}

void QWebPage::paintEvent(QPaintEvent *ev)
{
#ifdef QWEBKIT_TIME_RENDERING
    QTime time;
    time.start();
#endif

    QPainter p(this);

    QVector<QRect> vector = ev->region().rects();
    if (!vector.isEmpty()) {
        for (int i = 0; i < vector.size(); ++i) {
            mainFrame()->render(&p, vector.at(i));
        }
    } else {
        mainFrame()->render(&p, ev->rect());
    }

#ifdef    QWEBKIT_TIME_RENDERING
    int elapsed = time.elapsed();
    qDebug()<<"paint event on "<<ev->region()<<", took to render =  "<<elapsed;
#endif
}

void QWebPage::mouseMoveEvent(QMouseEvent *ev)
{
    mainFrame()->mouseMoveEvent(ev);
}

void QWebPage::mousePressEvent(QMouseEvent *ev)
{
    mainFrame()->mousePressEvent(ev);
}

void QWebPage::mouseDoubleClickEvent(QMouseEvent *ev)
{
    mainFrame()->mouseDoubleClickEvent(ev);
}

void QWebPage::mouseReleaseEvent(QMouseEvent *ev)
{
    mainFrame()->mouseReleaseEvent(ev);
}

void QWebPage::wheelEvent(QWheelEvent *ev)
{
    mainFrame()->wheelEvent(ev);
    if (!ev->isAccepted())
        QWidget::wheelEvent(ev);
}

void QWebPage::keyPressEvent(QKeyEvent *ev)
{
    PlatformKeyboardEvent kevent(ev, false);

    if (!mainFrame()->d->eventHandler)
        return;

    bool handled = mainFrame()->d->eventHandler->keyEvent(kevent);
    if (handled) {
    } else {
        handled = true;
        PlatformScrollbar *h, *v;
        h = mainFrame()->d->horizontalScrollBar();
        v = mainFrame()->d->verticalScrollBar();
        if (!h || !v)
          return;

        switch (ev->key()) {
            case Qt::Key_Up:
                v->setValue(v->value() - 10);
                update();
                break;
            case Qt::Key_Down:
                v->setValue(v->value() + 10);
                update();
                break;
            case Qt::Key_Left:
                h->setValue(h->value() - 10);
                update();
                break;
            case Qt::Key_Right:
                h->setValue(h->value() + 10);
                update();
                break;
            case Qt::Key_PageUp:
                v->setValue(v->value() - height());
                update();
                break;
            case Qt::Key_PageDown:
                v->setValue(v->value() + height());
                update();
                break;
            default:
                handled = false;
                break;
        }
    }

    ev->setAccepted(handled);
}

void QWebPage::keyReleaseEvent(QKeyEvent *ev)
{
    if (ev->isAutoRepeat()) {
        ev->setAccepted(true);
        return;
    }

    PlatformKeyboardEvent kevent(ev, true);

    if (!mainFrame()->d->eventHandler)
        return;

    bool handled = mainFrame()->d->eventHandler->keyEvent(kevent);
    ev->setAccepted(handled);
}

void QWebPage::focusInEvent(QFocusEvent *ev)
{
    if (ev->reason() != Qt::PopupFocusReason) {
        mainFrame()->d->frame->page()->focusController()->setFocusedFrame(mainFrame()->d->frame);
        mainFrame()->d->frame->setIsActive(true);
    }
    QWidget::focusInEvent(ev);
}

void QWebPage::focusOutEvent(QFocusEvent *ev)
{
    QWidget::focusOutEvent(ev);
    if (ev->reason() != Qt::PopupFocusReason) {
        mainFrame()->d->frame->selectionController()->clear();
        mainFrame()->d->frame->setIsActive(false);
    }
}

bool QWebPage::focusNextPrevChild(bool next)
{
    Q_UNUSED(next)
    return false;
}

void QWebPage::dragEnterEvent(QDragEnterEvent *ev)
{
#ifndef QT_NO_DRAGANDDROP
    DragData dragData(ev->mimeData(), ev->pos(), QCursor::pos(), 
                      dropActionToDragOp(ev->possibleActions()));
    Qt::DropAction action = dragOpToDropAction(d->page->dragController()->dragEntered(&dragData));
    ev->setDropAction(action);
    ev->accept();
#endif
}

void QWebPage::dragLeaveEvent(QDragLeaveEvent *ev)
{
#ifndef QT_NO_DRAGANDDROP
    DragData dragData(0, IntPoint(), QCursor::pos(), DragOperationNone);
    d->page->dragController()->dragExited(&dragData);
    ev->accept();
#endif
}

void QWebPage::dragMoveEvent(QDragMoveEvent *ev)
{
#ifndef QT_NO_DRAGANDDROP
    DragData dragData(ev->mimeData(), ev->pos(), QCursor::pos(), 
                      dropActionToDragOp(ev->possibleActions()));
    Qt::DropAction action = dragOpToDropAction(d->page->dragController()->dragUpdated(&dragData));
    ev->setDropAction(action);
    ev->accept();
#endif
}

void QWebPage::dropEvent(QDropEvent *ev)
{
#ifndef QT_NO_DRAGANDDROP
    DragData dragData(ev->mimeData(), ev->pos(), QCursor::pos(), 
                      dropActionToDragOp(ev->possibleActions()));
    Qt::DropAction action = dragOpToDropAction(d->page->dragController()->performDrag(&dragData));
    ev->accept();
#endif
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

QPixmap QWebPage::icon() const
{
    Image* image = iconDatabase()->iconForPageURL(url().toString(), IntSize(16, 16));
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

QString QWebPage::chooseFile(QWebFrame *parentFrame, const QString& oldFile)
{
    //FIXME frame pos...
#ifndef QT_NO_FILEDIALOG
    return QFileDialog::getOpenFileName(this, QString::null, oldFile);
#else
    return QString::null;
#endif
}

#ifndef QT_NO_NETWORKPROXY
void QWebPage::setNetworkProxy(const QNetworkProxy& proxy)
{
    d->networkProxy = proxy;
}

QNetworkProxy QWebPage::networkProxy() const
{
    return d->networkProxy;
}
#endif

