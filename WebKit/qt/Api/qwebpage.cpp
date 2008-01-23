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
#include "qwebview.h"
#include "qwebframe.h"
#include "qwebpage_p.h"
#include "qwebframe_p.h"
#include "qwebhistory.h"
#include "qwebhistory_p.h"
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
#if QT_VERSION >= 0x040400
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#else
#include "qwebnetworkinterface.h"
#endif

using namespace WebCore;

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

QWebPagePrivate::QWebPagePrivate(QWebPage *qq)
    : q(qq)
    , view(0)
    , modified(false)
{
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
#if QT_VERSION < 0x040400
    networkInterface = 0;
#else
    networkManager = 0;
#endif
    insideOpenCall = false;

    history.d = new QWebHistoryPrivate(page->backForwardList());
    memset(actions, 0, sizeof(actions));
}

QWebPagePrivate::~QWebPagePrivate()
{
    delete undoStack;
    delete settings;
    delete page;
#if QT_VERSION >= 0x040400
    delete networkManager;
#endif
}

#if QT_VERSION < 0x040400
QWebPage::NavigationRequestResponse QWebPagePrivate::navigationRequested(QWebFrame *frame, const QWebNetworkRequest &request, QWebPage::NavigationType type)
{
    if (insideOpenCall
        && frame == mainFrame)
        return QWebPage::AcceptNavigationRequest;
    return q->navigationRequested(frame, request, type);
}
#else
QWebPage::NavigationRequestResponse QWebPagePrivate::navigationRequested(QWebFrame *frame, const QNetworkRequest &request, QWebPage::NavigationType type)
{
    if (insideOpenCall
        && frame == mainFrame)
        return QWebPage::AcceptNavigationRequest;
    return q->navigationRequested(frame, request, type);
}
#endif

void QWebPagePrivate::createMainFrame()
{
    if (!mainFrame) {
        QWebFrameData frameData;
        frameData.ownerElement = 0;
        frameData.allowsScrolling = true;
        frameData.marginWidth = 0;
        frameData.marginHeight = 0;
        mainFrame = new QWebFrame(q, &frameData);
        mainFrame->d->frameView->setFrameGeometry(IntRect(IntPoint(0,0), q->viewportSize()));

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
            case WebCore::CheckableActionType: /* fall through */
            case WebCore::ActionType: {
                QWebPage::WebAction action = webActionForContextMenuAction(item.action());
                QAction *a = q->action(action);
                if (a) {
                    ContextMenuItem it(item);
                    webcoreMenu->checkOrEnableIfNeeded(it);
                    PlatformMenuItemDescription desc = it.releasePlatformDescription();
                    a->setEnabled(desc.enabled);
                    a->setChecked(desc.checked);
                    a->setCheckable(item.type() == WebCore::CheckableActionType);

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

void QWebPagePrivate::mouseMoveEvent(QMouseEvent *ev)
{
    QWebFramePrivate::core(mainFrame)->eventHandler()->mouseMoved(PlatformMouseEvent(ev, 0));
}

void QWebPagePrivate::mousePressEvent(QMouseEvent *ev)
{
    QWebFramePrivate::core(mainFrame)->eventHandler()->handleMousePressEvent(PlatformMouseEvent(ev, 1));
}

void QWebPagePrivate::mouseDoubleClickEvent(QMouseEvent *ev)
{
    QWebFramePrivate::core(mainFrame)->eventHandler()->handleMousePressEvent(PlatformMouseEvent(ev, 2));
}

void QWebPagePrivate::mouseReleaseEvent(QMouseEvent *ev)
{
    QWebFramePrivate::core(mainFrame)->eventHandler()->handleMouseReleaseEvent(PlatformMouseEvent(ev, 0));
}

void QWebPagePrivate::contextMenuEvent(QContextMenuEvent *ev)
{
    page->contextMenuController()->clearContextMenu();

    WebCore::Frame* focusedFrame = page->focusController()->focusedOrMainFrame();
    focusedFrame->eventHandler()->sendContextMenuEvent(PlatformMouseEvent(ev, 1));
    ContextMenu *menu = page->contextMenuController()->contextMenu();
    // If the website defines its own handler then sendContextMenuEvent takes care of
    // calling/showing it and the context menu pointer will be zero. This is the case
    // on maps.google.com for example.
    if (!menu)
        return;

    QWebPageContext oldContext = currentContext;
    currentContext = QWebPageContext(menu->hitTestResult());

    const QList<ContextMenuItem> *items = menu->platformDescription();
    QMenu *qmenu = createContextMenu(menu, items);
    if (qmenu) {
        qmenu->exec(ev->globalPos());
        delete qmenu;
    }
    currentContext = oldContext;
}

void QWebPagePrivate::wheelEvent(QWheelEvent *ev)
{
    WebCore::PlatformWheelEvent pev(ev);
    bool accepted = QWebFramePrivate::core(mainFrame)->eventHandler()->handleWheelEvent(pev);
    ev->setAccepted(accepted);
}

void QWebPagePrivate::keyPressEvent(QKeyEvent *ev)
{
    bool handled = false;
    WebCore::Frame *frame = page->focusController()->focusedOrMainFrame();
    WebCore::Editor *editor = frame->editor();
    if (editor->canEdit()) {
        if (ev == QKeySequence::Cut) {
            q->triggerAction(QWebPage::Cut);
            handled = true;
        } else if (ev == QKeySequence::Copy) {
            q->triggerAction(QWebPage::Copy);
            handled = true;
        } else if (ev == QKeySequence::Paste) {
            q->triggerAction(QWebPage::Paste);
            handled = true;
        } else if (ev == QKeySequence::Undo) {
            q->triggerAction(QWebPage::Undo);
            handled = true;
        } else if (ev == QKeySequence::Redo) {
            q->triggerAction(QWebPage::Redo);
            handled = true;
        } else if(ev == QKeySequence::MoveToNextChar) {
            q->triggerAction(QWebPage::MoveToNextChar);
            handled = true;
        } else if(ev == QKeySequence::MoveToPreviousChar) {
            q->triggerAction(QWebPage::MoveToPreviousChar);
            handled = true;
        } else if(ev == QKeySequence::MoveToNextWord) {
            q->triggerAction(QWebPage::MoveToNextWord);
            handled = true;
        } else if(ev == QKeySequence::MoveToPreviousWord) {
            q->triggerAction(QWebPage::MoveToPreviousWord);
            handled = true;
        } else if(ev == QKeySequence::MoveToNextLine) {
            q->triggerAction(QWebPage::MoveToNextLine);
            handled = true;
        } else if(ev == QKeySequence::MoveToPreviousLine) {
            q->triggerAction(QWebPage::MoveToPreviousLine);
            handled = true;
//             } else if(ev == QKeySequence::MoveToNextPage) {
//             } else if(ev == QKeySequence::MoveToPreviousPage) {
        } else if(ev == QKeySequence::MoveToStartOfLine) {
            q->triggerAction(QWebPage::MoveToStartOfLine);
            handled = true;
        } else if(ev == QKeySequence::MoveToEndOfLine) {
            q->triggerAction(QWebPage::MoveToEndOfLine);
            handled = true;
        } else if(ev == QKeySequence::MoveToStartOfBlock) {
            q->triggerAction(QWebPage::MoveToStartOfBlock);
            handled = true;
        } else if(ev == QKeySequence::MoveToEndOfBlock) {
            q->triggerAction(QWebPage::MoveToEndOfBlock);
            handled = true;
        } else if(ev == QKeySequence::MoveToStartOfDocument) {
            q->triggerAction(QWebPage::MoveToStartOfDocument);
            handled = true;
        } else if(ev == QKeySequence::MoveToEndOfDocument) {
            q->triggerAction(QWebPage::MoveToEndOfDocument);
            handled = true;
        } else if(ev == QKeySequence::SelectNextChar) {
            q->triggerAction(QWebPage::SelectNextChar);
            handled = true;
        } else if(ev == QKeySequence::SelectPreviousChar) {
            q->triggerAction(QWebPage::SelectPreviousChar);
            handled = true;
        } else if(ev == QKeySequence::SelectNextWord) {
            q->triggerAction(QWebPage::SelectNextWord);
            handled = true;
        } else if(ev == QKeySequence::SelectPreviousWord) {
            q->triggerAction(QWebPage::SelectPreviousWord);
            handled = true;
        } else if(ev == QKeySequence::SelectNextLine) {
            q->triggerAction(QWebPage::SelectNextLine);
            handled = true;
        } else if(ev == QKeySequence::SelectPreviousLine) {
            q->triggerAction(QWebPage::SelectPreviousLine);
            handled = true;
//             } else if(ev == QKeySequence::SelectNextPage) {
//             } else if(ev == QKeySequence::SelectPreviousPage) {
        } else if(ev == QKeySequence::SelectStartOfLine) {
            q->triggerAction(QWebPage::SelectStartOfLine);
            handled = true;
        } else if(ev == QKeySequence::SelectEndOfLine) {
            q->triggerAction(QWebPage::SelectEndOfLine);
            handled = true;
        } else if(ev == QKeySequence::SelectStartOfBlock) {
            q->triggerAction(QWebPage::SelectStartOfBlock);
            handled = true;
        } else if(ev == QKeySequence::SelectEndOfBlock) {
            q->triggerAction(QWebPage::SelectEndOfBlock);
            handled = true;
        } else if(ev == QKeySequence::SelectStartOfDocument) {
            q->triggerAction(QWebPage::SelectStartOfDocument);
            handled = true;
        } else if(ev == QKeySequence::SelectEndOfDocument) {
            q->triggerAction(QWebPage::SelectEndOfDocument);
            handled = true;
        } else if(ev == QKeySequence::DeleteStartOfWord) {
            q->triggerAction(QWebPage::DeleteStartOfWord);
            handled = true;
        } else if(ev == QKeySequence::DeleteEndOfWord) {
            q->triggerAction(QWebPage::DeleteEndOfWord);
            handled = true;
//             } else if(ev == QKeySequence::DeleteEndOfLine) {
        }
    }
    if (!handled)
        handled = frame->eventHandler()->keyEvent(ev);
    if (!handled) {
        handled = true;
        PlatformScrollbar *h, *v;
        h = mainFrame->d->horizontalScrollBar();
        v = mainFrame->d->verticalScrollBar();
        QFont defaultFont;
        if (view)
            defaultFont = view->font();
        QFontMetrics fm(defaultFont);
        int fontHeight = fm.height();
        if (ev == QKeySequence::MoveToNextPage
            || ev->key() == Qt::Key_Space) {
            if (v)
                v->setValue(v->value() + q->viewportSize().height() - fontHeight);
        } else if (ev == QKeySequence::MoveToPreviousPage) {
            if (v)
                v->setValue(v->value() - q->viewportSize().height() + fontHeight);
        } else if (ev->key() == Qt::Key_Up && ev->modifiers() == Qt::ControlModifier) {
            if (v)
                v->setValue(0);
        } else if (ev->key() == Qt::Key_Down && ev->modifiers() == Qt::ControlModifier) {
            if (v)
                v->setValue(INT_MAX);
        } else {
            switch (ev->key()) {
            case Qt::Key_Up:
                if (v)
                    v->setValue(v->value() - fontHeight);
                break;
            case Qt::Key_Down:
                if (v)
                    v->setValue(v->value() + fontHeight);
                break;
            case Qt::Key_Left:
                if (h)
                    h->setValue(h->value() - fontHeight);
                break;
            case Qt::Key_Right:
                if (h)
                    h->setValue(h->value() + fontHeight);
                break;
            case Qt::Key_Backspace:
                if (ev->modifiers() == Qt::ShiftModifier)
                    q->triggerAction(QWebPage::GoForward);
                else
                    q->triggerAction(QWebPage::GoBack);
            default:
                handled = false;
                break;
            }
        }
    }

    ev->setAccepted(handled);
}

void QWebPagePrivate::keyReleaseEvent(QKeyEvent *ev)
{
    if (ev->isAutoRepeat()) {
        ev->setAccepted(true);
        return;
    }

    WebCore::Frame* frame = page->focusController()->focusedOrMainFrame();
    bool handled = frame->eventHandler()->keyEvent(ev);
    ev->setAccepted(handled);
}

void QWebPagePrivate::focusInEvent(QFocusEvent *ev)
{
    if (ev->reason() != Qt::PopupFocusReason)
        page->focusController()->setFocusedFrame(QWebFramePrivate::core(mainFrame));
}

void QWebPagePrivate::focusOutEvent(QFocusEvent *ev)
{
    if (ev->reason() != Qt::PopupFocusReason)
        page->focusController()->setFocusedFrame(0);
}

void QWebPagePrivate::dragEnterEvent(QDragEnterEvent *ev)
{
#ifndef QT_NO_DRAGANDDROP
    DragData dragData(ev->mimeData(), ev->pos(), QCursor::pos(),
                      dropActionToDragOp(ev->possibleActions()));
    Qt::DropAction action = dragOpToDropAction(page->dragController()->dragEntered(&dragData));
    ev->setDropAction(action);
    ev->accept();
#endif
}

void QWebPagePrivate::dragLeaveEvent(QDragLeaveEvent *ev)
{
#ifndef QT_NO_DRAGANDDROP
    DragData dragData(0, IntPoint(), QCursor::pos(), DragOperationNone);
    page->dragController()->dragExited(&dragData);
    ev->accept();
#endif
}

void QWebPagePrivate::dragMoveEvent(QDragMoveEvent *ev)
{
#ifndef QT_NO_DRAGANDDROP
    DragData dragData(ev->mimeData(), ev->pos(), QCursor::pos(),
                      dropActionToDragOp(ev->possibleActions()));
    Qt::DropAction action = dragOpToDropAction(page->dragController()->dragUpdated(&dragData));
    ev->setDropAction(action);
    ev->accept();
#endif
}

void QWebPagePrivate::dropEvent(QDropEvent *ev)
{
#ifndef QT_NO_DRAGANDDROP
    DragData dragData(ev->mimeData(), ev->pos(), QCursor::pos(),
                      dropActionToDragOp(ev->possibleActions()));
    Qt::DropAction action = dragOpToDropAction(page->dragController()->performDrag(&dragData));
    ev->accept();
#endif
}

/*!
    \enum QWebPage::WebAction

    \value NoWebAction No action is triggered.
    \value OpenLink Open the current link.
    \value OpenLinkInNewWindow Open the current link in a new window.
    \value OpenFrameInNewWindow Replicate the current frame in a new window.
    \value DownloadLinkToDisk Download the current link to the disk.
    \value CopyLinkToClipboard Copy the current link to the clipboard.
    \value OpenImageInNewWindow Open the highlighted image in a new window.
    \value DownloadImageToDisk Download the highlighted image to the disk.
    \value CopyImageToClipboard Copy the highlighted image to the clipboard.
    \value GoBack Navigate back in the history of navigated links.
    \value GoForward Navigate forward in the history of navigated links.
    \value Stop Stop loading the current page.
    \value Reload Reload the current page.
    \value Cut Cut the content currently selected into the clipboard.
    \value Copy Copy the content currently selected into the clipboard.
    \value Paste Paste content from the clipboard.
    \value Undo Undo the last editing action.
    \value Redo Redo the last editing action.
    \value MoveToNextChar Move the cursor to the next character.
    \value MoveToPreviousChar Move the cursor to the previous character.
    \value MoveToNextWord Move the cursor to the next word.
    \value MoveToPreviousWord Move the cursor to the previous word.
    \value MoveToNextLine Move the cursor to the next line.
    \value MoveToPreviousLine Move the cursor to the previous line.
    \value MoveToStartOfLine Move the cursor to the start of the line.
    \value MoveToEndOfLine Move the cursor to the end of the line.
    \value MoveToStartOfBlock Move the cursor to the start of the block.
    \value MoveToEndOfBlock Move the cursor to the end of the block.
    \value MoveToStartOfDocument Move the cursor to the start of the document.
    \value MoveToEndOfDocument Move the cursor to the end of the document.
    \value SelectNextChar Select to the next character.
    \value SelectPreviousChar Select to the previous character.
    \value SelectNextWord Select to the next word.
    \value SelectPreviousWord Select to the previous word.
    \value SelectNextLine Select to the next line.
    \value SelectPreviousLine Select to the previous line.
    \value SelectStartOfLine Select to the start of the line.
    \value SelectEndOfLine Select to the end of the line.
    \value SelectStartOfBlock Select to the start of the block.
    \value SelectEndOfBlock Select to the end of the block.
    \value SelectStartOfDocument Select to the start of the document.
    \value SelectEndOfDocument Select to the end of the document.
    \value DeleteStartOfWord Delete to the start of the word.
    \value DeleteEndOfWord Delete to the end of the word.
    \value SetTextDirectionDefault Set the text direction to the default direction.
    \value SetTextDirectionLeftToRight Set the text direction to left-to-right.
    \value SetTextDirectionRightToLeft Set the text direction to right-to-left.
    \value ToggleBold Toggle the formatting between bold and normal weight.
    \value ToggleItalic Toggle the formatting between italic and normal style.
    \value ToggleUnderline Toggle underlining.
    \value InspectElement Show the Web Inspector with the currently highlighted HTML element.
    \omitvalue WebActionCount

*/

/*!
    \class QWebPage
    \since 4.4
    \brief The QWebPage class provides a widget that is used to view and edit web documents.

    QWebPage holds a main frame responsible for web content, settings, the history
    of navigated links as well as actions. This class can be used, together with QWebFrame,
    if you want to provide functionality like QWebView in a setup without widgets.
*/

/*!
    Constructs an empty QWebView with parent \a parent.
*/
QWebPage::QWebPage(QObject *parent)
    : QObject(parent)
    , d(new QWebPagePrivate(this))
{
    setView(qobject_cast<QWidget *>(parent));

    connect(this, SIGNAL(loadProgressChanged(int)), this, SLOT(_q_onLoadProgressChanged(int)));
}

/*!
    Destructor.
*/
QWebPage::~QWebPage()
{
    FrameLoader *loader = d->mainFrame->d->frame->loader();
    if (loader)
        loader->detachFromParent();
    delete d;
}

/*!
    Returns the main frame of the page.

    The main frame provides access to the hierarchy of sub-frames and is also needed if you
    want to explicitly render a web page into a given painter.
*/
QWebFrame *QWebPage::mainFrame() const
{
    d->createMainFrame();
    return d->mainFrame;
}

/*!
    Returns the frame currently active.
*/
QWebFrame *QWebPage::currentFrame() const
{
    return static_cast<WebCore::FrameLoaderClientQt *>(d->page->focusController()->focusedOrMainFrame()->loader()->client())->webFrame();
}

/*!
    Returns a pointer to the view's history of navigated web pages.

*/
QWebHistory *QWebPage::history() const
{
    return &d->history;
}

/*!
    Sets the \a view that is associated with the web page.

    \sa view()
*/
void QWebPage::setView(QWidget *view)
{
    d->view = view;
    setViewportSize(view ? view->size() : QSize(0, 0));
}

/*!
    Returns the view widget that is associated with the web page.

    \sa setView()
*/
QWidget *QWebPage::view() const
{
    return d->view;
}


/*!
    This function is called whenever a JavaScript program tries to print to what is the console in web browsers.
*/
void QWebPage::javaScriptConsoleMessage(const QString& message, unsigned int lineNumber, const QString& sourceID)
{
}

/*!
    This function is called whenever a JavaScript program calls the alert() function.
*/
void QWebPage::javaScriptAlert(QWebFrame *frame, const QString& msg)
{
    QMessageBox::information(d->view, mainFrame()->title(), msg, QMessageBox::Ok);
}

/*!
    This function is called whenever a JavaScript program calls the confirm() function.
*/
bool QWebPage::javaScriptConfirm(QWebFrame *frame, const QString& msg)
{
    return 0 == QMessageBox::information(d->view, mainFrame()->title(), msg, QMessageBox::Yes, QMessageBox::No);
}

/*!
    This function is called whenever a JavaScript program tries to prompt the user of input.
*/
bool QWebPage::javaScriptPrompt(QWebFrame *frame, const QString& msg, const QString& defaultValue, QString* result)
{
    bool ok = false;
#ifndef QT_NO_INPUTDIALOG
    QString x = QInputDialog::getText(d->view, mainFrame()->title(), msg, QLineEdit::Normal, defaultValue, &ok);
    if (ok && result) {
        *result = x;
    }
#endif
    return ok;
}

/*!
    This function is called whenever WebKit wants to create a new window, for example as a result of
    a JavaScript request to open a document in a new window.
*/
QWebPage *QWebPage::createWindow()
{
    QWebView *webView = qobject_cast<QWebView *>(d->view);
    if (webView) {
        QWebView *newView = webView->createWindow();
        if (newView)
            return newView->page();
    }
    return 0;
}

/*!
    This function is called whenever WebKit wants to create a new window that should act as a modal dialog.
*/
QWebPage *QWebPage::createModalDialog()
{
    return 0;
}

/*!
    This function is called whenever WebKit encounters a HTML object element with type "application/x-qt-plugin".
    The \a classid, \a url, \a paramNames and \a paramValues correspond to the HTML object element attributes and
    child elements to configure the embeddable object.
*/
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
    WebCore::ResourceRequest rr(url, frame->loader()->outgoingReferrer());
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

/*!
    This function can be called to trigger the specified \a action.
    It is also called by QtWebKit if the user triggers the action, for example
    through a context menu item.

    If \a action is a checkable action then \a checked specified whether the action
    is toggled or not.
*/
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
        case CopyLinkToClipboard:
            editor->copyURL(d->currentContext.linkUrl(), d->currentContext.text());
            break;
        case OpenImageInNewWindow:
            openNewWindow(d->currentContext.imageUrl(), frame);
            break;
        case DownloadImageToDisk:
        case DownloadLinkToDisk:
            frame->loader()->client()->startDownload(WebCore::ResourceRequest(d->currentContext.linkUrl(), frame->loader()->outgoingReferrer()));
            break;
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

QSize QWebPage::viewportSize() const
{
    QWebFrame *frame = mainFrame();
    if (frame->d->frame && frame->d->frameView)
        return frame->d->frameView->frameGeometry().size();
    return QSize(0, 0);
}

/*!
    \property QWebPage::viewportSize

    Specifies the size of the viewport. The size affects for example the visibility of scrollbars
    if the document is larger than the viewport.
*/
void QWebPage::setViewportSize(const QSize &size) const
{
    QWebFrame *frame = mainFrame();
    if (frame->d->frame && frame->d->frameView) {
        frame->d->frameView->setFrameGeometry(QRect(QPoint(0, 0), size));
        frame->d->frame->forceLayout();
        frame->d->frame->view()->adjustViewSize();
    }
}


#if QT_VERSION < 0x040400
QWebPage::NavigationRequestResponse QWebPage::navigationRequested(QWebFrame *frame, const QWebNetworkRequest &request, QWebPage::NavigationType type)
#else
QWebPage::NavigationRequestResponse QWebPage::navigationRequested(QWebFrame *frame, const QNetworkRequest &request, QWebPage::NavigationType type)
#endif
{
    Q_UNUSED(request)
    return AcceptNavigationRequest;
}

/*!
    \property QWebPage::selectedText

    Returns the text currently selected.
*/
QString QWebPage::selectedText() const
{
    return d->page->focusController()->focusedOrMainFrame()->selectedText();
}

/*!
   Returns a QAction for the specified WebAction \a action.

   The action is owned by the QWebPage but you can customize the look by
   changing its properties.

   QWebPage also takes care of implementing the action, so that upon
   triggering the corresponding action is performed on the page.
*/
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
    \property QWebPage::modified

    Specifies if the page contains unsubmitted form data.
*/
bool QWebPage::isModified() const
{
    return d->modified;
}


/*!
    Returns a pointer to the undo stack used for editable content.
*/
QUndoStack *QWebPage::undoStack() const
{
    if (!d->undoStack)
        d->undoStack = new QUndoStack(const_cast<QWebPage *>(this));

    return d->undoStack;
}

/*! \reimp
*/
bool QWebPage::event(QEvent *ev)
{
    switch (ev->type()) {
    case QEvent::MouseMove:
        d->mouseMoveEvent(static_cast<QMouseEvent*>(ev));
        break;
    case QEvent::MouseButtonPress:
        d->mousePressEvent(static_cast<QMouseEvent*>(ev));
        break;
    case QEvent::MouseButtonDblClick:
        d->mouseDoubleClickEvent(static_cast<QMouseEvent*>(ev));
        break;
    case QEvent::MouseButtonRelease:
        d->mouseReleaseEvent(static_cast<QMouseEvent*>(ev));
        break;
    case QEvent::ContextMenu:
        d->contextMenuEvent(static_cast<QContextMenuEvent*>(ev));
        break;
    case QEvent::Wheel:
        d->wheelEvent(static_cast<QWheelEvent*>(ev));
        break;
    case QEvent::KeyPress:
        d->keyPressEvent(static_cast<QKeyEvent*>(ev));
        break;
    case QEvent::KeyRelease:
        d->keyReleaseEvent(static_cast<QKeyEvent*>(ev));
        break;
    case QEvent::FocusIn:
        d->focusInEvent(static_cast<QFocusEvent*>(ev));
        break;
    case QEvent::FocusOut:
        d->focusOutEvent(static_cast<QFocusEvent*>(ev));
        break;
#ifndef QT_NO_DRAGANDDROP
    case QEvent::DragEnter:
        d->dragEnterEvent(static_cast<QDragEnterEvent*>(ev));
        break;
    case QEvent::DragLeave:
        d->dragLeaveEvent(static_cast<QDragLeaveEvent*>(ev));
        break;
    case QEvent::DragMove:
        d->dragMoveEvent(static_cast<QDragMoveEvent*>(ev));
        break;
    case QEvent::Drop:
        d->dropEvent(static_cast<QDropEvent*>(ev));
        break;
#endif
    default:
        return QObject::event(ev);
    }

    return true;
}

/*!
    Similar to QWidget::focusNextPrevChild it focuses the next focusable web element
    if \a next is true. Otherwise the previous element is focused.
*/
bool QWebPage::focusNextPrevChild(bool next)
{
    QKeyEvent ev(QEvent::KeyPress, Qt::Key_Tab, Qt::KeyboardModifiers(next ? Qt::NoModifier : Qt::ShiftModifier));
    d->keyPressEvent(&ev);
    bool hasFocusedNode = false;
    Frame *frame = d->page->focusController()->focusedFrame();
    if (frame) {
        Document *document = frame->document();
        hasFocusedNode = document && document->focusedNode();
    }
    //qDebug() << "focusNextPrevChild(" << next << ") =" << ev.isAccepted() << "focusedNode?" << hasFocusedNode;
    return hasFocusedNode;
}

/*!
    Returns a pointe to the page's settings object.
*/
QWebSettings *QWebPage::settings()
{
    return d->settings;
}

/*!
    This function is called when the web content requests a file name, for example
    as a result of the user clicking on a "file upload" button in a HTML form.
*/
QString QWebPage::chooseFile(QWebFrame *parentFrame, const QString& oldFile)
{
#ifndef QT_NO_FILEDIALOG
    return QFileDialog::getOpenFileName(d->view, QString::null, oldFile);
#else
    return QString::null;
#endif
}

#if QT_VERSION < 0x040400

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

#else

/*!
    Sets the QNetworkAccessManager \a manager that is responsible for serving network
    requests for this QWebPage.

    \sa networkAccessManager
*/
void QWebPage::setNetworkAccessManager(QNetworkAccessManager *manager)
{
    if (manager == d->networkManager)
        return;
    delete d->networkManager;
    d->networkManager = manager;
}

/*!
    Returns the QNetworkAccessManager \a manager that is responsible for serving network
    requests for this QWebPage.

    \sa setNetworkAccessManager
*/
QNetworkAccessManager *QWebPage::networkAccessManager() const
{
    if (!d->networkManager) {
        QWebPage *that = const_cast<QWebPage *>(this);
        that->d->networkManager = new QNetworkAccessManager(that);
    }
    return d->networkManager;
}

#endif

/*!
    This function is called when a user agent for HTTP requests is needed. You can re-implement this
    function to dynamically return different user agent's for different urls, based on the \a url parameter.
*/
QString QWebPage::userAgentFor(const QUrl& url) const
{
    Q_UNUSED(url)
    return QLatin1String("Mozilla/5.0 (Macintosh; U; Intel Mac OS X; en) AppleWebKit/523.15 (KHTML, like Gecko) Safari/419.3 Qt");
}


void QWebPagePrivate::_q_onLoadProgressChanged(int) {
    m_totalBytes = page->progress()->totalPageAndResourceBytesToLoad();
    m_bytesReceived = page->progress()->totalBytesReceived();
}


/*!
    Returns the total number of bytes that were received from the network to render the current page,
    including extra content such as embedded images.
*/
quint64 QWebPage::totalBytes() const {
    return d->m_bytesReceived;
}


/*!
    Returns the number of bytes that were received from the network to render the current page.
*/
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

/*!
    \fn void QWebPage::loadProgressChanged(int progress)

    This signal is emitted when the global progress status changes.
    The current value is provided by \a progress in percent.
    It accumulates changes from all the child frames.
*/

/*!
    \fn void QWebPage::hoveringOverLink(const QString &link, const QString &title, const QString &textContent)

    This signal is emitted when the mouse is hovering over a link.
    The first parameter is the \a link url, the second is the link \a title
    if any, and third \a textContent is the text content. Method is emitter with both
    empty parameters when the mouse isn't hovering over any link element.
*/

/*!
    \fn void QWebPage::statusBarTextChanged(const QString& text)

    This signal is emitted when the statusbar \a text is changed by the page.
*/

/*!
    \fn void QWebPage::frameCreated(QWebFrame *frame)

    This signal is emitted whenever the page creates a new \a frame.
*/

/*!
    \fn void QWebPage::selectionChanged()

    This signal is emitted whenever the selection changes.
*/

/*!
    \fn void QWebPage::geometryChangeRequest(const QRect& geom)

    This signal is emitted whenever the document wants to change the position and size of the
    page to \a geom. This can happen for example through JavaScript.
*/

/*!
    \fn void QWebPage::handleUnsupportedContent(QNetworkReply *reply)

    This signals is emitted when webkit cannot handle a link the user navigated to.

    At signal emissions time the meta data of the QNetworkReply is available.
*/

/*!
    \fn void QWebPage::download(const QNetworkRequest &request)

    This signals is emitted when the user decides to download a link.
*/

#include "moc_qwebpage.cpp"
