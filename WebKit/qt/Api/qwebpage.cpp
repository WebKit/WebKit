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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/
#include "config.h"
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
#include "ContextMenu.h"
#include "ContextMenuClientQt.h"
#include "DragClientQt.h"
#include "DragController.h"
#include "DragData.h"
#include "EditorClientQt.h"
#include "Settings.h"
#include "Page.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "KURL.h"
#include "Image.h"
#include "IconDatabase.h"
#include "InspectorClientQt.h"
#include "FocusController.h"
#include "Editor.h"
#include "PlatformScrollBar.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformWheelEvent.h"
#include "ProgressTracker.h"
#include "RefPtr.h"
#include "HashMap.h"
#include "HitTestResult.h"
#include "LocalizedStrings.h"

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
    : q(qq)
    , modified(false)
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

    history.d = new QWebPageHistoryPrivate(page->backForwardList());
    memset(actions, 0, sizeof(actions));
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
        mainFrame = new QWebFrame(q, &frameData);
        QObject::connect(mainFrame, SIGNAL(titleChanged(const QString&)),
                q, SIGNAL(titleChanged(const QString&)));
        QObject::connect(mainFrame, SIGNAL(hoveringOverLink(const QString&, const QString&)),
                q, SIGNAL(hoveringOverLink(const QString&, const QString&)));
        
        mainFrame->d->frameView->setFrameGeometry(q->geometry());

        emit q->frameCreated(mainFrame);
    }
}

static QWebPage::WebAction webActionForContextMenuAction(WebCore::ContextMenuAction action)
{
    switch (action) {
        case WebCore::ContextMenuItemTagOpenLink: return QWebPage::OpenLink;
        case WebCore::ContextMenuItemTagOpenLinkInNewWindow: return QWebPage::OpenLinkInNewWindow;
        case WebCore::ContextMenuItemTagDownloadLinkToDisk: return QWebPage::DownloadLinkToDisk;
        case WebCore::ContextMenuItemTagCopyLinkToClipboard: return QWebPage::CopyLinkToClipboard;
        case WebCore::ContextMenuItemTagOpenImageInNewWindow: return QWebPage::OpenImageInNewWindow;
        case WebCore::ContextMenuItemTagDownloadImageToDisk: return QWebPage::DownloadImageToDisk;
        case WebCore::ContextMenuItemTagCopyImageToClipboard: return QWebPage::CopyImageToClipboard;
        case WebCore::ContextMenuItemTagOpenFrameInNewWindow: return QWebPage::OpenFrameInNewWindow;
        case WebCore::ContextMenuItemTagCopy: return QWebPage::Copy;
        case WebCore::ContextMenuItemTagGoBack: return QWebPage::GoBack;
        case WebCore::ContextMenuItemTagGoForward: return QWebPage::GoForward;
        case WebCore::ContextMenuItemTagStop: return QWebPage::Stop;
        case WebCore::ContextMenuItemTagReload: return QWebPage::Reload;
        case WebCore::ContextMenuItemTagCut: return QWebPage::Cut;
        case WebCore::ContextMenuItemTagPaste: return QWebPage::Paste;
        default: break;
    }
    return QWebPage::NoWebAction;
}

QMenu *QWebPagePrivate::createContextMenu(QList<WebCore::ContextMenuItem> *items)
{
    QMenu *menu = new QMenu;
    for (int i = 0; i < items->count(); ++i) {
        const ContextMenuItem &item = items->at(i);
        switch (item.type()) {
            case WebCore::ActionType: {
                QWebPage::WebAction action = webActionForContextMenuAction(item.action());
                QAction *a = q->webAction(action);
                if (a)
                    menu->addAction(a);
                break;
            }
            case WebCore::SeparatorType:
                menu->addSeparator();
                break;
            case WebCore::SubmenuType: {
                QMenu *subMenu = createContextMenu(item.platformSubMenu());
                subMenu->setTitle(item.title());
                menu->addAction(subMenu->menuAction());
                break;
            }
        }
    }
    return menu;
}

QWebFrame *QWebPagePrivate::frameAt(const QPoint &pos) const
{
    QWebFrame *frame = mainFrame;

redo:
    QList<QWebFrame*> children = frame->childFrames();
    for (int i = 0; i < children.size(); ++i) {
        if (children.at(i)->geometry().contains(pos)) {
            frame = children.at(i);
            goto redo;
        }
    }
    if (frame->geometry().contains(pos))
        return frame;
    return 0;
}

void QWebPagePrivate::_q_webActionTriggered(bool checked)
{
    QAction *a = qobject_cast<QAction *>(q->sender());
    if (!a)
        return;
    QWebPage::WebAction action = static_cast<QWebPage::WebAction>(a->data().toInt());
    q->webActionTriggered(action, checked);
}

void QWebPagePrivate::updateAction(QWebPage::WebAction action)
{
    QAction *a = actions[action];
    if (!a || !mainFrame)
        return;

    WebCore::FrameLoader *loader = mainFrame->d->frame->loader();
    WebCore::Editor *editor = page->focusController()->focusedOrMainFrame()->editor();

    bool enabled = a->isEnabled();

    switch (action) {
        case QWebPage::GoBack:
            enabled = loader->canGoBackOrForward(-1);
            break;
        case QWebPage::GoForward:
            enabled = loader->canGoBackOrForward(1);
            break;
        case QWebPage::Stop:
            enabled = loader->isLoading();
            break;
        case QWebPage::Reload:
            enabled = !loader->isLoading();
            break;
        case QWebPage::Cut:
            enabled = editor->canCut();
            break;
        case QWebPage::Copy:
            enabled = editor->canCopy();
            break;
        case QWebPage::Paste:
            enabled = editor->canPaste();
            break;
        case QWebPage::Undo:
        case QWebPage::Redo:
            // those two are handled by QUndoStack
            break;
        default: break;
    }

    a->setEnabled(enabled);
}

void QWebPagePrivate::updateNavigationActions()
{
    updateAction(QWebPage::GoBack);
    updateAction(QWebPage::GoForward);
    updateAction(QWebPage::Stop);
    updateAction(QWebPage::Reload);
}

void QWebPagePrivate::updateEditorActions()
{
    updateAction(QWebPage::Cut);
    updateAction(QWebPage::Copy);
    updateAction(QWebPage::Paste);
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
    connect(this, SIGNAL(loadProgressChanged(int)), this, SLOT(_q_onLoadProgressChanged(int)));
}

QWebPage::~QWebPage()
{
    FrameLoader *loader = d->mainFrame->d->frame->loader();
    if (loader)
        loader->detachFromParent();
    delete d;
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
    webActionTriggered(Stop);
}

QWebPageHistory *QWebPage::history() const
{
    return &d->history;
}

void QWebPage::goBack()
{
    webActionTriggered(GoBack);
}

void QWebPage::goForward()
{
    webActionTriggered(GoForward);
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

QWebPage *QWebPage::createModalDialog()
{
    return 0;
}

QObject *QWebPage::createPlugin(const QString &classid, const QUrl &url, const QStringList &paramNames, const QStringList &paramValues)
{
    Q_UNUSED(classid)
    Q_UNUSED(url)
    Q_UNUSED(paramNames)
    Q_UNUSED(paramValues)
    return 0;
}

static WebCore::FrameLoadRequest frameLoadRequest(const QUrl &url, WebCore::Frame *frame)
{
    WebCore::ResourceRequest rr(WebCore::KURL(url.toString()),
                                frame->loader()->outgoingReferrer());
    return WebCore::FrameLoadRequest(rr);
}

static void openNewWindow(const QUrl& url, WebCore::Frame* frame)
{
    if (Page* oldPage = frame->page())
        if (Page* newPage = oldPage->chrome()->createWindow(frame,
                frameLoadRequest(url, frame)))
            newPage->chrome()->show();
}

void QWebPage::webActionTriggered(WebAction action, bool checked)
{
    WebCore::Frame *frame = d->page->focusController()->focusedOrMainFrame();
    WebCore::Editor *editor = frame->editor();
    const char *command = 0;

    switch (action) {
        case OpenLink:
            if (QWebFrame *targetFrame = d->currentContext.targetFrame()) {
                WTF::RefPtr<WebCore::Frame> wcFrame = targetFrame->d->frame;
                targetFrame->d->frame->loader()->load(frameLoadRequest(d->currentContext.linkUrl(), wcFrame.get()),
                                                      /*lockHistory*/ false,
                                                      /*userGesture*/ true,
                                                      /*event*/ 0,
                                                      /*HTMLFormElement*/ 0,
                                                      /*formValues*/
                                                      WTF::HashMap<String, String>());
                break;
            } else {
            }
            // fall through
        case OpenLinkInNewWindow:
            openNewWindow(d->currentContext.linkUrl(), frame);
            break;
        case OpenFrameInNewWindow:
            break;
        case DownloadLinkToDisk:
        case CopyLinkToClipboard:
            editor->copyURL(WebCore::KURL(d->currentContext.linkUrl().toString()), d->currentContext.text());
            break;
        case OpenImageInNewWindow:
            openNewWindow(d->currentContext.imageUrl(), frame);
            break;
        case DownloadImageToDisk:
        case CopyImageToClipboard:
            break;
        case GoBack:
            d->page->goBack();
            break;
        case GoForward:
            d->page->goForward();
            break;
        case Stop:
            mainFrame()->d->frame->loader()->stopForUserCancel();
            break;
        case Reload:
            mainFrame()->d->frame->loader()->reload();
            break;
        case Cut:
            editor->cut();
            break;
        case Copy:
            editor->copy();
            break;
        case Paste:
            editor->paste();
            break;

        case Undo:
            editor->undo();
            break;
        case Redo:
            editor->redo();
            break;

        case MoveToNextChar:
            command = "MoveForward";
            break;
        case MoveToPreviousChar:
            command = "MoveBackward";
            break;
        case MoveToNextWord:
            command = "MoveWordForward";
            break;
        case MoveToPreviousWord:
            command = "MoveWordBackward";
            break;
        case MoveToNextLine:
            command = "MoveDown";
            break;
        case MoveToPreviousLine:
            command = "MoveUp";
            break;
        case MoveToStartOfLine:
            command = "MoveToBeginningOfLine";
            break;
        case MoveToEndOfLine:
            command = "MoveToEndOfLine";
            break;
        case MoveToStartOfBlock:
            command = "MoveToBeginningOfParagraph";
            break;
        case MoveToEndOfBlock:
            command = "MoveToEndOfParagraph";
            break;
        case MoveToStartOfDocument:
            command = "MoveToBeginningOfDocument";
            break;
        case MoveToEndOfDocument:
            command = "MoveToEndOfDocument";
            break;
        case SelectNextChar:
            command = "MoveForwardAndModifySelection";
            break;
        case SelectPreviousChar:
            command = "MoveBackwardAndModifySelection";
            break;
        case SelectNextWord:
            command = "MoveWordForwardAndModifySelection";
            break;
        case SelectPreviousWord:
            command = "MoveWordBackwardAndModifySelection";
            break;
        case SelectNextLine:
            command = "MoveDownAndModifySelection";
            break;
        case SelectPreviousLine:
            command = "MoveUpAndModifySelection";
            break;
        case SelectStartOfLine:
            command = "MoveToBeginningOfLineAndModifySelection";
            break;
        case SelectEndOfLine:
            command = "MoveToEndOfLineAndModifySelection";
            break;
        case SelectStartOfBlock:
            command = "MoveToBeginningOfParagraphAndModifySelection";
            break;
        case SelectEndOfBlock:
            command = "MoveToEndOfParagraphAndModifySelection";
            break;
        case SelectStartOfDocument:
            command = "MoveToBeginningOfDocumentAndModifySelection";
            break;
        case SelectEndOfDocument:
            command = "MoveToEndOfDocumentAndModifySelection";
            break;
        case DeleteStartOfWord:
            command = "DeleteWordBackward";
            break;
        case DeleteEndOfWord:
            command = "DeleteWordForward";
            break;

        default: break;
    }

    if (command)
        editor->execCommand(command);
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
    webActionTriggered(Cut);
}

void QWebPage::copy()
{
    webActionTriggered(Copy);
}

void QWebPage::paste()
{
    webActionTriggered(Paste);
}

QAction *QWebPage::webAction(WebAction action) const
{
    if (action == QWebPage::NoWebAction) return 0;
    if (d->actions[action])
        return d->actions[action];

    QString text;

    switch (action) {
        case OpenLink:
            text = contextMenuItemTagOpenLink();
            break;
        case OpenLinkInNewWindow:
            text = contextMenuItemTagOpenLinkInNewWindow();
            break;
        case OpenFrameInNewWindow:
            text = contextMenuItemTagOpenFrameInNewWindow();
            break;

        case DownloadLinkToDisk:
            text = contextMenuItemTagDownloadLinkToDisk();
            break;
        case CopyLinkToClipboard:
            text = contextMenuItemTagCopyLinkToClipboard();
            break;

        case OpenImageInNewWindow:
            text = contextMenuItemTagOpenImageInNewWindow();
            break;
        case DownloadImageToDisk:
            text = contextMenuItemTagDownloadImageToDisk();
            break;
        case CopyImageToClipboard:
            text = contextMenuItemTagCopyImageToClipboard();
            break;

        case GoBack:
            text = contextMenuItemTagGoBack();
            break;
        case GoForward:
            text = contextMenuItemTagGoForward();
            break;
        case Stop:
            text = contextMenuItemTagStop();
            break;
        case Reload:
            text = contextMenuItemTagReload();
            break;

        case Cut:
            text = contextMenuItemTagCut();
            break;
        case Copy:
            text = contextMenuItemTagCopy();
            break;
        case Paste:
            text = contextMenuItemTagPaste();
            break;

        case Undo: {
            QAction *a = undoStack()->createUndoAction(d->q);
            d->actions[action] = a;
            return a;
        }
        case Redo: {
            QAction *a = undoStack()->createRedoAction(d->q);
            d->actions[action] = a;
            return a;
        }
        case MoveToNextChar:
        case MoveToPreviousChar:
        case MoveToNextWord:
        case MoveToPreviousWord:
        case MoveToNextLine:
        case MoveToPreviousLine:
        case MoveToStartOfLine:
        case MoveToEndOfLine:
        case MoveToStartOfBlock:
        case MoveToEndOfBlock:
        case MoveToStartOfDocument:
        case MoveToEndOfDocument:
        case SelectNextChar:
        case SelectPreviousChar:
        case SelectNextWord:
        case SelectPreviousWord:
        case SelectNextLine:
        case SelectPreviousLine:
        case SelectStartOfLine:
        case SelectEndOfLine:
        case SelectStartOfBlock:
        case SelectEndOfBlock:
        case SelectStartOfDocument:
        case SelectEndOfDocument:
        case DeleteStartOfWord:
        case DeleteEndOfWord:
            break; // ####

        case NoWebAction:
            return 0;
    }

    if (text.isEmpty())
        return 0;

    QAction *a = new QAction(d->q);
    a->setText(text);
    a->setData(action);

    connect(a, SIGNAL(triggered(bool)),
            this, SLOT(_q_webActionTriggered(bool)));

    d->actions[action] = a;
    d->updateAction(action);
    return a;
}

/*!
  Returns true if the page contains unsubmitted form data.
*/
bool QWebPage::isModified() const
{
    return d->modified;
}


QUndoStack *QWebPage::undoStack() const
{
    if (!d->undoStack)
        d->undoStack = new QUndoStack(const_cast<QWebPage *>(this));

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
    QWebFramePrivate *frame = d->currentFrame(ev->pos())->d;
    if (!frame->frameView)
        return;

    frame->eventHandler->handleMouseMoveEvent(PlatformMouseEvent(ev, 0));
    const int xOffset =
        frame->horizontalScrollBar() ? frame->horizontalScrollBar()->value() : 0;
    const int yOffset =
        frame->verticalScrollBar() ? frame->verticalScrollBar()->value() : 0;
    IntPoint pt(ev->x() + xOffset, ev->y() + yOffset);
    WebCore::HitTestResult result = frame->eventHandler->hitTestResultAtPoint(pt, false);
    WebCore::Element *link = result.URLElement();
    if (link != frame->lastHoverElement) {
        frame->lastHoverElement = link;
        emit hoveringOverLink(result.absoluteLinkURL().prettyURL(), result.title());
    }
}

void QWebPage::mousePressEvent(QMouseEvent *ev)
{
    d->frameUnderMouse = d->frameAt(ev->pos());
    QWebFramePrivate *frame = d->frameUnderMouse->d;
    if (!frame->eventHandler)
        return;

    frame->eventHandler->handleMousePressEvent(PlatformMouseEvent(ev, 1));

    //FIXME need to keep track of subframe focus for key events!
    frame->page->setFocus();
}

void QWebPage::mouseDoubleClickEvent(QMouseEvent *ev)
{
    QWebFramePrivate *frame = d->currentFrame(ev->pos())->d;
    if (!frame->eventHandler)
        return;

    frame->eventHandler->handleMousePressEvent(PlatformMouseEvent(ev, 2));

    //FIXME need to keep track of subframe focus for key events!
    frame->page->setFocus();
}

void QWebPage::mouseReleaseEvent(QMouseEvent *ev)
{
    QWebFramePrivate *frame = d->currentFrame(ev->pos())->d;
    if (frame->frameView) {
        frame->eventHandler->handleMouseReleaseEvent(PlatformMouseEvent(ev, 0));

        //FIXME need to keep track of subframe focus for key events!
        frame->page->setFocus();
    }
    d->frameUnderMouse = 0;
}

void QWebPage::contextMenuEvent(QContextMenuEvent *ev)
{
    QWebFramePrivate *frame = d->currentFrame(ev->pos())->d;
    if (!frame->eventHandler)
        return;
    d->page->contextMenuController()->clearContextMenu();
    frame->eventHandler->sendContextMenuEvent(PlatformMouseEvent(ev, 1));
    ContextMenu *menu = d->page->contextMenuController()->contextMenu();

    QWebPageContext oldContext = d->currentContext;
    d->currentContext = QWebPageContext(menu->hitTestResult());

    QList<ContextMenuItem> *items = menu->platformDescription();
    QMenu *qmenu = d->createContextMenu(items);
    if (qmenu) {
        qmenu->exec(ev->globalPos());
        delete qmenu;
    }
    d->currentContext = oldContext;
}

void QWebPage::wheelEvent(QWheelEvent *ev)
{
    QWebFramePrivate *frame = d->currentFrame(ev->pos())->d;

    bool accepted = false;
    if (frame->eventHandler) {
        WebCore::PlatformWheelEvent pev(ev);
        accepted = frame->eventHandler->handleWheelEvent(pev);
    }

    ev->setAccepted(accepted);

    //FIXME need to keep track of subframe focus for key events!
    frame->page->setFocus();

    if (!ev->isAccepted())
        QWidget::wheelEvent(ev);
}

void QWebPage::keyPressEvent(QKeyEvent *ev)
{
    if (!mainFrame()->d->eventHandler)
        return;

    bool handled = false;
    QWebFrame *frame = mainFrame();
    WebCore::Editor *editor = frame->d->frame->editor();
    if (editor->canEdit()) {
        if (ev == QKeySequence::Cut) {
            webActionTriggered(Cut);
            handled = true;
        } else if (ev == QKeySequence::Copy) {
            webActionTriggered(Copy);
            handled = true;
        } else if (ev == QKeySequence::Paste) {
            webActionTriggered(Paste);
            handled = true;
        } else if (ev == QKeySequence::Undo) {
            webActionTriggered(Undo);
            handled = true;
        } else if (ev == QKeySequence::Redo) {
            webActionTriggered(Redo);
            handled = true;
        } else if(ev == QKeySequence::MoveToNextChar) {
            webActionTriggered(MoveToNextChar);
            handled = true;
        } else if(ev == QKeySequence::MoveToPreviousChar) {
            webActionTriggered(MoveToPreviousChar);
            handled = true;
        } else if(ev == QKeySequence::MoveToNextWord) {
            webActionTriggered(MoveToNextWord);
            handled = true;
        } else if(ev == QKeySequence::MoveToPreviousWord) {
            webActionTriggered(MoveToPreviousWord);
            handled = true;
        } else if(ev == QKeySequence::MoveToNextLine) {
            webActionTriggered(MoveToNextLine);
            handled = true;
        } else if(ev == QKeySequence::MoveToPreviousLine) {
            webActionTriggered(MoveToPreviousLine);
            handled = true;
//             } else if(ev == QKeySequence::MoveToNextPage) {
//             } else if(ev == QKeySequence::MoveToPreviousPage) {
        } else if(ev == QKeySequence::MoveToStartOfLine) {
            webActionTriggered(MoveToStartOfLine);
            handled = true;
        } else if(ev == QKeySequence::MoveToEndOfLine) {
            webActionTriggered(MoveToEndOfLine);
            handled = true;
        } else if(ev == QKeySequence::MoveToStartOfBlock) {
            webActionTriggered(MoveToStartOfBlock);
            handled = true;
        } else if(ev == QKeySequence::MoveToEndOfBlock) {
            webActionTriggered(MoveToEndOfBlock);
            handled = true;
        } else if(ev == QKeySequence::MoveToStartOfDocument) {
            webActionTriggered(MoveToStartOfDocument);
            handled = true;
        } else if(ev == QKeySequence::MoveToEndOfDocument) {
            webActionTriggered(MoveToEndOfDocument);
            handled = true;
        } else if(ev == QKeySequence::SelectNextChar) {
            webActionTriggered(SelectNextChar);
            handled = true;
        } else if(ev == QKeySequence::SelectPreviousChar) {
            webActionTriggered(SelectPreviousChar);
            handled = true;
        } else if(ev == QKeySequence::SelectNextWord) {
            webActionTriggered(SelectNextWord);
            handled = true;
        } else if(ev == QKeySequence::SelectPreviousWord) {
            webActionTriggered(SelectPreviousWord);
            handled = true;
        } else if(ev == QKeySequence::SelectNextLine) {
            webActionTriggered(SelectNextLine);
            handled = true;
        } else if(ev == QKeySequence::SelectPreviousLine) {
            webActionTriggered(SelectPreviousLine);
            handled = true;
//             } else if(ev == QKeySequence::SelectNextPage) {
//             } else if(ev == QKeySequence::SelectPreviousPage) {
        } else if(ev == QKeySequence::SelectStartOfLine) {
            webActionTriggered(SelectStartOfLine);
            handled = true;
        } else if(ev == QKeySequence::SelectEndOfLine) {
            webActionTriggered(SelectEndOfLine);
            handled = true;
        } else if(ev == QKeySequence::SelectStartOfBlock) {
            webActionTriggered(SelectStartOfBlock);
            handled = true;
        } else if(ev == QKeySequence::SelectEndOfBlock) {
            webActionTriggered(SelectEndOfBlock);
            handled = true;
        } else if(ev == QKeySequence::SelectStartOfDocument) {
            webActionTriggered(SelectStartOfDocument);
            handled = true;
        } else if(ev == QKeySequence::SelectEndOfDocument) {
            webActionTriggered(SelectEndOfDocument);
            handled = true;
        } else if(ev == QKeySequence::DeleteStartOfWord) {
            webActionTriggered(DeleteStartOfWord);
            handled = true;
        } else if(ev == QKeySequence::DeleteEndOfWord) {
            webActionTriggered(DeleteEndOfWord);
            handled = true;
//             } else if(ev == QKeySequence::DeleteEndOfLine) {
        }
    }
    if (!handled) 
        handled = frame->d->eventHandler->keyEvent(ev);
    if (!handled) {
        handled = true;
        PlatformScrollbar *h, *v;
        h = mainFrame()->d->horizontalScrollBar();
        v = mainFrame()->d->verticalScrollBar();

        if (ev == QKeySequence::MoveToNextPage) {
            if (v)
                v->setValue(v->value() + height());
        } else if (ev == QKeySequence::MoveToPreviousPage) {
            if (v)
                v->setValue(v->value() - height());
        } else {
            switch (ev->key()) {
            case Qt::Key_Up:
                if (v)
                    v->setValue(v->value() - 10);
                break;
            case Qt::Key_Down:
                if (v)
                    v->setValue(v->value() + 10);
                break;
            case Qt::Key_Left:
                if (h)
                    h->setValue(h->value() - 10);
                break;
            case Qt::Key_Right:
                if (h)
                    h->setValue(h->value() + 10);
                break;
            default:
                handled = false;
                break;
            }
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

    if (!mainFrame()->d->eventHandler)
        return;

    bool handled = mainFrame()->d->eventHandler->keyEvent(ev);
    ev->setAccepted(handled);
}

void QWebPage::focusInEvent(QFocusEvent *ev)
{
    if (ev->reason() != Qt::PopupFocusReason) 
        mainFrame()->d->frame->page()->focusController()->setFocusedFrame(mainFrame()->d->frame);
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

    // ### should be configurable
    wSettings->setDefaultTextEncodingName("iso-8859-1");
    wSettings->setDOMPasteAllowed(true);
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

QString QWebPage::userAgentStringForUrl(const QUrl& forUrl) const {
    Q_UNUSED(forUrl)
    return QLatin1String("Mozilla/5.0 (Macintosh; U; Intel Mac OS X; en) AppleWebKit/418.9.1 (KHTML, like Gecko) Safari/419.3 Qt");
}


void QWebPagePrivate::_q_onLoadProgressChanged(int) {
    m_totalBytes = page->progress()->totalPageAndResourceBytesToLoad();
    m_bytesReceived = page->progress()->totalBytesReceived();
}


quint64 QWebPage::totalBytes() const {
    return d->m_bytesReceived;
}


quint64 QWebPage::bytesReceived() const {
    return d->m_totalBytes;
}

QWebPageContext::QWebPageContext(const WebCore::HitTestResult &hitTest)
    : d(new QWebPageContextPrivate)
{
    d->pos = hitTest.point();
    d->text = hitTest.textContent();
    d->linkUrl = hitTest.absoluteLinkURL().url();
    d->imageUrl = hitTest.absoluteImageURL().url();
    WebCore::Image *img = hitTest.image();
    if (img) {
        QPixmap *pix = img->getPixmap();
        if (pix)
            d->image = *pix;
    }
    WebCore::Frame *frame = hitTest.targetFrame();
    if (frame)
        d->targetFrame = frame->view()->qwebframe();
}

QWebPageContext::QWebPageContext()
    : d(0)
{
}

QWebPageContext::QWebPageContext(const QWebPageContext &other)
    : d(0)
{
    if (other.d)
        d = new QWebPageContextPrivate(*other.d);
}

QWebPageContext &QWebPageContext::operator=(const QWebPageContext &other)
{
    if (this != &other) {
        if (other.d) {
            if (!d)
                d = new QWebPageContextPrivate;
            *d = *other.d;
        } else {
            delete d;
            d = 0;
        }
    }
    return *this;
}

QWebPageContext::~QWebPageContext()
{
    delete d;
}

QPoint QWebPageContext::pos() const
{
    if (!d)
        return QPoint();
    return d->pos;
}

QString QWebPageContext::text() const
{
    if (!d)
        return QString();
    return d->text;
}

QUrl QWebPageContext::linkUrl() const
{
    if (!d)
        return QUrl();
    return d->linkUrl;
}

QUrl QWebPageContext::imageUrl() const
{
    if (!d)
        return QUrl();
    return d->linkUrl;
}

QPixmap QWebPageContext::image() const
{
    if (!d)
        return QPixmap();
    return d->image;
}

QWebFrame *QWebPageContext::targetFrame() const
{
    if (!d)
        return 0;
    return d->targetFrame;
}

#include "moc_qwebpage.cpp"
