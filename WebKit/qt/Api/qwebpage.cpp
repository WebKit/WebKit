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
#include "FrameLoaderClientQt.h"
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
#include "InspectorController.h"
#include "FocusController.h"
#include "Editor.h"
#include "PlatformScrollBar.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformWheelEvent.h"
#include "ProgressTracker.h"
#include "RefPtr.h"
#include "HashMap.h"
#include "HitTestResult.h"
#include "WindowFeatures.h"
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
                    new DragClientQt(q), new InspectorClientQt(q));

    // ### should be configurable
    page->settings()->setDefaultTextEncodingName("iso-8859-1");

    settings = new QWebSettings(page->settings());

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
    delete settings;
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
        case WebCore::ContextMenuItemTagDefaultDirection: return QWebPage::SetTextDirectionDefault;
        case WebCore::ContextMenuItemTagLeftToRight: return QWebPage::SetTextDirectionLeftToRight;
        case WebCore::ContextMenuItemTagRightToLeft: return QWebPage::SetTextDirectionRightToLeft;
        case WebCore::ContextMenuItemTagBold: return QWebPage::ToggleBold;
        case WebCore::ContextMenuItemTagItalic: return QWebPage::ToggleItalic;
        case WebCore::ContextMenuItemTagUnderline: return QWebPage::ToggleUnderline;
        case WebCore::ContextMenuItemTagInspectElement: return QWebPage::InspectElement;
        default: break;
    }
    return QWebPage::NoWebAction;
}

QMenu *QWebPagePrivate::createContextMenu(const WebCore::ContextMenu *webcoreMenu, const QList<WebCore::ContextMenuItem> *items)
{
    QMenu *menu = new QMenu;
    for (int i = 0; i < items->count(); ++i) {
        const ContextMenuItem &item = items->at(i);
        switch (item.type()) {
            case WebCore::ActionType: {
                QWebPage::WebAction action = webActionForContextMenuAction(item.action());
                QAction *a = q->action(action);
                if (a) {
                    ContextMenuItem it(item);
                    webcoreMenu->checkOrEnableIfNeeded(it);
                    PlatformMenuItemDescription desc = it.releasePlatformDescription();
                    a->setEnabled(desc.enabled);
                    a->setChecked(desc.checked);

                    menu->addAction(a);
                }
                break;
            }
            case WebCore::SeparatorType:
                menu->addSeparator();
                break;
            case WebCore::SubmenuType: {
                QMenu *subMenu = createContextMenu(webcoreMenu, item.platformSubMenu());
                if (!subMenu->actions().isEmpty()) {
                    subMenu->setTitle(item.title());
                    menu->addAction(subMenu->menuAction());
                } else {
                    delete subMenu;
                }
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
    q->triggerAction(action, checked);
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

QUrl QWebPage::url() const
{
    return QUrl((QString)mainFrame()->d->frame->loader()->url().string());
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

QWebFrame *QWebPage::currentFrame() const
{
    return static_cast<WebCore::FrameLoaderClientQt *>(d->page->focusController()->focusedOrMainFrame()->loader()->client())->webFrame();
}

QWebPageHistory *QWebPage::history() const
{
    return &d->history;
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
    if (Page* oldPage = frame->page()) {
        WindowFeatures features;
        if (Page* newPage = oldPage->chrome()->createWindow(frame,
                frameLoadRequest(url, frame), features))
            newPage->chrome()->show();
    }
}

void QWebPage::triggerAction(WebAction action, bool checked)
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
            command = "Cut";
            break;
        case Copy:
            command = "Copy";
            break;
        case Paste:
            command = "Paste";
            break;

        case Undo:
            command = "Undo";
            break;
        case Redo:
            command = "Redo";
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

        case SetTextDirectionDefault:
            editor->setBaseWritingDirection("inherit");
            break;
        case SetTextDirectionLeftToRight:
            editor->setBaseWritingDirection("ltr");
            break;
        case SetTextDirectionRightToLeft:
            editor->setBaseWritingDirection("rtl");
            break;

        case ToggleBold:
            command = "ToggleBold";
            break;
        case ToggleItalic:
            command = "ToggleItalic";
            break;
        case ToggleUnderline:
            editor->toggleUnderline();

        case InspectElement:
            d->page->inspectorController()->inspect(d->currentContext.d->innerNonSharedNode.get());
            break;

        default: break;
    }

    if (command)
        editor->command(command).execute();
}

QWebPage::NavigationRequestResponse QWebPage::navigationRequested(QWebFrame *frame, const QWebNetworkRequest &request, QWebPage::NavigationType type)
{
    Q_UNUSED(request)
    return AcceptNavigationRequest;
}

QString QWebPage::selectedText() const
{
    return d->page->focusController()->focusedOrMainFrame()->selectedText();
}

QAction *QWebPage::action(WebAction action) const
{
    if (action == QWebPage::NoWebAction) return 0;
    if (d->actions[action])
        return d->actions[action];

    QString text;
    bool checkable = false;

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

        case SetTextDirectionDefault:
            text = contextMenuItemTagDefaultDirection();
            break;
        case SetTextDirectionLeftToRight:
            text = contextMenuItemTagLeftToRight();
            checkable = true;
            break;
        case SetTextDirectionRightToLeft:
            text = contextMenuItemTagRightToLeft();
            checkable = true;
            break;

        case ToggleBold:
            text = contextMenuItemTagBold();
            checkable = true;
            break;
        case ToggleItalic:
            text = contextMenuItemTagItalic();
            checkable = true;
            break;
        case ToggleUnderline:
            text = contextMenuItemTagUnderline();
            checkable = true;
            break;

        case InspectElement:
            text = contextMenuItemTagInspectElement();
            break;

        case NoWebAction:
            return 0;
    }

    if (text.isEmpty())
        return 0;

    QAction *a = new QAction(d->q);
    a->setText(text);
    a->setData(action);
    a->setCheckable(checkable);

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

    mainFrame()->layout();
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
    QWebFrame *f = d->currentFrame(ev->pos());
    if (!f)
        return;

    QWebFramePrivate *frame = f->d;
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
        emit hoveringOverLink(result.absoluteLinkURL().prettyURL(), result.title(), result.textContent());
    }
}

void QWebPage::mousePressEvent(QMouseEvent *ev)
{
    d->frameUnderMouse = d->frameAt(ev->pos());
    if (!d->frameUnderMouse)
        return;

    QWebFramePrivate *frame = d->frameUnderMouse->d;
    if (!frame->eventHandler)
        return;

    frame->eventHandler->handleMousePressEvent(PlatformMouseEvent(ev, 1));

    //FIXME need to keep track of subframe focus for key events!
    frame->page->setFocus();
}

void QWebPage::mouseDoubleClickEvent(QMouseEvent *ev)
{
    QWebFrame *f = d->currentFrame(ev->pos());
    if (!f)
        return;

    QWebFramePrivate *frame = f->d;
    if (!frame->eventHandler)
        return;

    frame->eventHandler->handleMousePressEvent(PlatformMouseEvent(ev, 2));

    //FIXME need to keep track of subframe focus for key events!
    frame->page->setFocus();
}

void QWebPage::mouseReleaseEvent(QMouseEvent *ev)
{
    QWebFrame *f = d->currentFrame(ev->pos());
    if (!f)
        return;

    QWebFramePrivate *frame = f->d;
    if (!frame->frameView)
        return;

    frame->eventHandler->handleMouseReleaseEvent(PlatformMouseEvent(ev, 0));

    //FIXME need to keep track of subframe focus for key events!
    frame->page->setFocus();
    d->frameUnderMouse = 0;
}

void QWebPage::contextMenuEvent(QContextMenuEvent *ev)
{
    QWebFrame *f = d->currentFrame(ev->pos());
    if (!f)
        return;

    QWebFramePrivate *frame = f->d;
    if (!frame->eventHandler)
        return;

    d->page->contextMenuController()->clearContextMenu();
    frame->eventHandler->sendContextMenuEvent(PlatformMouseEvent(ev, 1));
    ContextMenu *menu = d->page->contextMenuController()->contextMenu();

    QWebPageContext oldContext = d->currentContext;
    d->currentContext = QWebPageContext(menu->hitTestResult());

    const QList<ContextMenuItem> *items = menu->platformDescription();
    QMenu *qmenu = d->createContextMenu(menu, items);
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
            triggerAction(Cut);
            handled = true;
        } else if (ev == QKeySequence::Copy) {
            triggerAction(Copy);
            handled = true;
        } else if (ev == QKeySequence::Paste) {
            triggerAction(Paste);
            handled = true;
        } else if (ev == QKeySequence::Undo) {
            triggerAction(Undo);
            handled = true;
        } else if (ev == QKeySequence::Redo) {
            triggerAction(Redo);
            handled = true;
        } else if(ev == QKeySequence::MoveToNextChar) {
            triggerAction(MoveToNextChar);
            handled = true;
        } else if(ev == QKeySequence::MoveToPreviousChar) {
            triggerAction(MoveToPreviousChar);
            handled = true;
        } else if(ev == QKeySequence::MoveToNextWord) {
            triggerAction(MoveToNextWord);
            handled = true;
        } else if(ev == QKeySequence::MoveToPreviousWord) {
            triggerAction(MoveToPreviousWord);
            handled = true;
        } else if(ev == QKeySequence::MoveToNextLine) {
            triggerAction(MoveToNextLine);
            handled = true;
        } else if(ev == QKeySequence::MoveToPreviousLine) {
            triggerAction(MoveToPreviousLine);
            handled = true;
//             } else if(ev == QKeySequence::MoveToNextPage) {
//             } else if(ev == QKeySequence::MoveToPreviousPage) {
        } else if(ev == QKeySequence::MoveToStartOfLine) {
            triggerAction(MoveToStartOfLine);
            handled = true;
        } else if(ev == QKeySequence::MoveToEndOfLine) {
            triggerAction(MoveToEndOfLine);
            handled = true;
        } else if(ev == QKeySequence::MoveToStartOfBlock) {
            triggerAction(MoveToStartOfBlock);
            handled = true;
        } else if(ev == QKeySequence::MoveToEndOfBlock) {
            triggerAction(MoveToEndOfBlock);
            handled = true;
        } else if(ev == QKeySequence::MoveToStartOfDocument) {
            triggerAction(MoveToStartOfDocument);
            handled = true;
        } else if(ev == QKeySequence::MoveToEndOfDocument) {
            triggerAction(MoveToEndOfDocument);
            handled = true;
        } else if(ev == QKeySequence::SelectNextChar) {
            triggerAction(SelectNextChar);
            handled = true;
        } else if(ev == QKeySequence::SelectPreviousChar) {
            triggerAction(SelectPreviousChar);
            handled = true;
        } else if(ev == QKeySequence::SelectNextWord) {
            triggerAction(SelectNextWord);
            handled = true;
        } else if(ev == QKeySequence::SelectPreviousWord) {
            triggerAction(SelectPreviousWord);
            handled = true;
        } else if(ev == QKeySequence::SelectNextLine) {
            triggerAction(SelectNextLine);
            handled = true;
        } else if(ev == QKeySequence::SelectPreviousLine) {
            triggerAction(SelectPreviousLine);
            handled = true;
//             } else if(ev == QKeySequence::SelectNextPage) {
//             } else if(ev == QKeySequence::SelectPreviousPage) {
        } else if(ev == QKeySequence::SelectStartOfLine) {
            triggerAction(SelectStartOfLine);
            handled = true;
        } else if(ev == QKeySequence::SelectEndOfLine) {
            triggerAction(SelectEndOfLine);
            handled = true;
        } else if(ev == QKeySequence::SelectStartOfBlock) {
            triggerAction(SelectStartOfBlock);
            handled = true;
        } else if(ev == QKeySequence::SelectEndOfBlock) {
            triggerAction(SelectEndOfBlock);
            handled = true;
        } else if(ev == QKeySequence::SelectStartOfDocument) {
            triggerAction(SelectStartOfDocument);
            handled = true;
        } else if(ev == QKeySequence::SelectEndOfDocument) {
            triggerAction(SelectEndOfDocument);
            handled = true;
        } else if(ev == QKeySequence::DeleteStartOfWord) {
            triggerAction(DeleteStartOfWord);
            handled = true;
        } else if(ev == QKeySequence::DeleteEndOfWord) {
            triggerAction(DeleteEndOfWord);
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

QWebSettings *QWebPage::settings()
{
    return d->settings;
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

QString QWebPage::userAgentFor(const QUrl& url) const {
    Q_UNUSED(url)
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
    d->linkUrl = hitTest.absoluteLinkURL().string();
    d->imageUrl = hitTest.absoluteImageURL().string();
    d->innerNonSharedNode = hitTest.innerNonSharedNode();
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
