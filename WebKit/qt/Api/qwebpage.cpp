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
#include "FrameTree.h"
#include "FrameLoaderClientQt.h"
#include "FrameView.h"
#include "ChromeClientQt.h"
#include "ContextMenu.h"
#include "ContextMenuClientQt.h"
#include "DocumentLoader.h"
#include "DragClientQt.h"
#include "DragController.h"
#include "DragData.h"
#include "EditorClientQt.h"
#include "Settings.h"
#include "Page.h"
#include "Pasteboard.h"
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

#include <QApplication>
#include <QBasicTimer>
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
#include <QClipboard>
#include <QSslSocket>
#include <QSysInfo>
#if QT_VERSION >= 0x040400
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#else
#include "qwebnetworkinterface.h"
#endif

using namespace WebCore;

// If you change this make sure to also adjust the docs for QWebPage::userAgentForUrl
#define WEBKIT_VERSION "523.15"

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
    , viewportSize(QSize(0,0))
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
    pluginFactory = 0;
#else
    networkManager = 0;
    pluginFactory = 0;
#endif
    insideOpenCall = false;
    forwardUnsupportedContent = false;
    linkPolicy = QWebPage::DontDelegateLinks;
    currentContextMenu = 0;

    history.d = new QWebHistoryPrivate(page->backForwardList());
    memset(actions, 0, sizeof(actions));
}

QWebPagePrivate::~QWebPagePrivate()
{
    delete currentContextMenu;
    delete undoStack;
    delete settings;
    delete page;
}

#if QT_VERSION < 0x040400
bool QWebPagePrivate::acceptNavigationRequest(QWebFrame *frame, const QWebNetworkRequest &request, QWebPage::NavigationType type)
{
    if (insideOpenCall
        && frame == mainFrame)
        return true;
    return q->acceptNavigationRequest(frame, request, type);
}
#else
bool QWebPagePrivate::acceptNavigationRequest(QWebFrame *frame, const QNetworkRequest &request, QWebPage::NavigationType type)
{
    if (insideOpenCall
        && frame == mainFrame)
        return true;
    return q->acceptNavigationRequest(frame, request, type);
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
        case WebCore::ContextMenuItemTagGoBack: return QWebPage::Back;
        case WebCore::ContextMenuItemTagGoForward: return QWebPage::Forward;
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

                bool anyEnabledAction = false;

                QList<QAction *> actions = subMenu->actions();
                for (int i = 0; i < actions.count(); ++i) {
                    if (actions.at(i)->isVisible())
                        anyEnabledAction |= actions.at(i)->isEnabled();
                }

                // don't show sub-menus with just disabled actions
                if (anyEnabledAction) {
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
        case QWebPage::Back:
            enabled = loader->canGoBackOrForward(-1);
            break;
        case QWebPage::Forward:
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
    updateAction(QWebPage::Back);
    updateAction(QWebPage::Forward);
    updateAction(QWebPage::Stop);
    updateAction(QWebPage::Reload);
}

void QWebPagePrivate::updateEditorActions()
{
    updateAction(QWebPage::Cut);
    updateAction(QWebPage::Copy);
    updateAction(QWebPage::Paste);
}

void QWebPagePrivate::timerEvent(QTimerEvent *ev)
{
    int timerId = ev->timerId();
    if (timerId == tripleClickTimer.timerId())
        tripleClickTimer.stop();
    else
        q->QObject::timerEvent(ev);
}

void QWebPagePrivate::mouseMoveEvent(QMouseEvent *ev)
{
    WebCore::Frame* frame = QWebFramePrivate::core(mainFrame);
    if (!frame->view())
        return;

    frame->eventHandler()->mouseMoved(PlatformMouseEvent(ev, 0));
}

void QWebPagePrivate::mousePressEvent(QMouseEvent *ev)
{
    WebCore::Frame* frame = QWebFramePrivate::core(mainFrame);
    if (!frame->view())
        return;

    if (tripleClickTimer.isActive() && (ev->pos() - tripleClick).manhattanLength() <
         QApplication::startDragDistance()) {
        mouseTripleClickEvent(ev);
        return;
    }
    
    frame->eventHandler()->handleMousePressEvent(PlatformMouseEvent(ev, 1));
}

void QWebPagePrivate::mouseDoubleClickEvent(QMouseEvent *ev)
{
    WebCore::Frame* frame = QWebFramePrivate::core(mainFrame);
    if (!frame->view())
        return;

    frame->eventHandler()->handleMousePressEvent(PlatformMouseEvent(ev, 2));
    
    tripleClickTimer.start(QApplication::doubleClickInterval(), q);
    tripleClick = ev->pos();
}

void QWebPagePrivate::mouseTripleClickEvent(QMouseEvent *ev)
{
    WebCore::Frame* frame = QWebFramePrivate::core(mainFrame);
    if (!frame->view())
        return;

    frame->eventHandler()->handleMousePressEvent(PlatformMouseEvent(ev, 3));
}

void QWebPagePrivate::mouseReleaseEvent(QMouseEvent *ev)
{
    WebCore::Frame* frame = QWebFramePrivate::core(mainFrame);
    if (!frame->view())
        return;

    frame->eventHandler()->handleMouseReleaseEvent(PlatformMouseEvent(ev, 0));

#ifndef QT_NO_CLIPBOARD
    if (QApplication::clipboard()->supportsSelection()) {
        bool oldSelectionMode = Pasteboard::generalPasteboard()->isSelectionMode();
        Pasteboard::generalPasteboard()->setSelectionMode(true);
        WebCore::Frame* focusFrame = page->focusController()->focusedOrMainFrame();
        if (ev->button() == Qt::LeftButton) {
            if(focusFrame && (focusFrame->editor()->canCopy() || focusFrame->editor()->canDHTMLCopy())) {
                focusFrame->editor()->copy();
            }
        } else if (ev->button() == Qt::MidButton) {
            if(focusFrame && (focusFrame->editor()->canPaste() || focusFrame->editor()->canDHTMLPaste())) {
                focusFrame->editor()->paste();
            }
        }
        Pasteboard::generalPasteboard()->setSelectionMode(oldSelectionMode);
    }
#endif
}

void QWebPagePrivate::contextMenuEvent(QContextMenuEvent *ev)
{
    if (currentContextMenu) {
        currentContextMenu->exec(ev->globalPos());
        delete currentContextMenu;
        currentContextMenu = 0;
    }
}

void QWebPagePrivate::wheelEvent(QWheelEvent *ev)
{
    WebCore::Frame* frame = QWebFramePrivate::core(mainFrame);
    if (!frame->view())
        return;

    WebCore::PlatformWheelEvent pev(ev);
    bool accepted = frame->eventHandler()->handleWheelEvent(pev);
    ev->setAccepted(accepted);
}

static QWebPage::WebAction editorActionForKeyEvent(QKeyEvent* event)
{
    static struct {
        QKeySequence::StandardKey standardKey;
        QWebPage::WebAction action;
    } editorActions[32] = {
        { QKeySequence::Cut, QWebPage::Cut },
        { QKeySequence::Copy, QWebPage::Copy },
        { QKeySequence::Paste, QWebPage::Paste },
        { QKeySequence::Undo, QWebPage::Undo },
        { QKeySequence::Redo, QWebPage::Redo },
        { QKeySequence::MoveToNextChar, QWebPage::MoveToNextChar },
        { QKeySequence::MoveToPreviousChar, QWebPage::MoveToPreviousChar },
        { QKeySequence::MoveToNextWord, QWebPage::MoveToNextWord },
        { QKeySequence::MoveToPreviousWord, QWebPage::MoveToPreviousWord },
        { QKeySequence::MoveToNextLine, QWebPage::MoveToNextLine },
        { QKeySequence::MoveToPreviousLine, QWebPage::MoveToPreviousLine },
        { QKeySequence::MoveToStartOfLine, QWebPage::MoveToStartOfLine },
        { QKeySequence::MoveToEndOfLine, QWebPage::MoveToEndOfLine },
        { QKeySequence::MoveToStartOfBlock, QWebPage::MoveToStartOfBlock },
        { QKeySequence::MoveToEndOfBlock, QWebPage::MoveToEndOfBlock },
        { QKeySequence::MoveToStartOfDocument, QWebPage::MoveToStartOfDocument },
        { QKeySequence::MoveToEndOfDocument, QWebPage::MoveToEndOfDocument },
        { QKeySequence::SelectNextChar, QWebPage::SelectNextChar },
        { QKeySequence::SelectPreviousChar, QWebPage::SelectPreviousChar },
        { QKeySequence::SelectNextWord, QWebPage::SelectNextWord },
        { QKeySequence::SelectPreviousWord, QWebPage::SelectPreviousWord },
        { QKeySequence::SelectNextLine, QWebPage::SelectNextLine },
        { QKeySequence::SelectPreviousLine, QWebPage::SelectPreviousLine },
        { QKeySequence::SelectStartOfLine, QWebPage::SelectStartOfLine },
        { QKeySequence::SelectEndOfLine, QWebPage::SelectEndOfLine },
        { QKeySequence::SelectStartOfBlock, QWebPage::SelectStartOfBlock },
        { QKeySequence::SelectEndOfBlock,  QWebPage::SelectEndOfBlock },
        { QKeySequence::SelectStartOfDocument, QWebPage::SelectStartOfDocument },
        { QKeySequence::SelectEndOfDocument, QWebPage::SelectEndOfDocument },
        { QKeySequence::DeleteStartOfWord, QWebPage::DeleteStartOfWord },
        { QKeySequence::DeleteEndOfWord, QWebPage::DeleteEndOfWord },
        { QKeySequence::UnknownKey, QWebPage::NoWebAction }
    };

    for (int i = 0; editorActions[i].standardKey != QKeySequence::UnknownKey; ++i)
        if (event == editorActions[i].standardKey)
            return editorActions[i].action;

    return QWebPage::NoWebAction;
}

void QWebPagePrivate::keyPressEvent(QKeyEvent *ev)
{
    bool handled = false;
    WebCore::Frame* frame = page->focusController()->focusedOrMainFrame();
    WebCore::Editor* editor = frame->editor();
    if (editor->canEdit()) {
        QWebPage::WebAction action = editorActionForKeyEvent(ev);
        if (action != QWebPage::NoWebAction) {
            q->triggerAction(action);
            handled = true;
        }
    }
    if (!handled)
        handled = frame->eventHandler()->keyEvent(ev);
    if (!handled) {
        handled = true;
        PlatformScrollbar *h, *v;
        h = q->currentFrame()->d->horizontalScrollBar();
        v = q->currentFrame()->d->verticalScrollBar();
        QFont defaultFont;
        if (view)
            defaultFont = view->font();
        QFontMetrics fm(defaultFont);
        int fontHeight = fm.height();
        if (ev == QKeySequence::MoveToNextPage
            || ev->key() == Qt::Key_Space) {
            if (v)
                v->setValue(v->value() + q->viewportSize().height() - fontHeight * 2);
        } else if (ev == QKeySequence::MoveToPreviousPage) {
            if (v)
                v->setValue(v->value() - q->viewportSize().height() + fontHeight * 2);
        } else if (ev->key() == Qt::Key_Up && ev->modifiers() & Qt::ControlModifier
                   || ev->key() == Qt::Key_Home) {
            if (v)
                v->setValue(0);
        } else if (ev->key() == Qt::Key_Down && ev->modifiers() & Qt::ControlModifier
                   || ev->key() == Qt::Key_End) {
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
                    q->triggerAction(QWebPage::Forward);
                else
                    q->triggerAction(QWebPage::Back);
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
    if (ev->reason() == Qt::PopupFocusReason)
        return;

    FocusController *focusController = page->focusController();
    Frame *frame = focusController->focusedFrame();
    if (frame) {
        focusController->setActive(true);
        frame->selectionController()->setFocused(true);
    } else {
        focusController->setFocusedFrame(QWebFramePrivate::core(mainFrame));
    }
}

void QWebPagePrivate::focusOutEvent(QFocusEvent *ev)
{
    if (ev->reason() == Qt::PopupFocusReason)
        return;

    // only set the focused frame inactive so that we stop painting the caret
    // and the focus frame. But don't tell the focus controller so that upon
    // focusInEvent() we can re-activate the frame.
    FocusController *focusController = page->focusController();
    Frame *frame = focusController->focusedFrame();
    if (frame) {
        focusController->setActive(false);
        frame->selectionController()->setFocused(false);
    }
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

void QWebPagePrivate::leaveEvent(QEvent *ev)
{
    // Fake a mouse move event just outside of the widget, since all
    // the interesting mouse-out behavior like invalidating scrollbars
    // is handled by the WebKit event handler's mouseMoved function.
    QMouseEvent fakeEvent(QEvent::MouseMove, QCursor::pos(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    mouseMoveEvent(&fakeEvent);
}

/*!
    \property QWebPage::palette
    \brief the page's palette

    The background brush of the palette is used to draw the background of the main frame.
*/
void QWebPage::setPalette(const QPalette &pal)
{
    d->palette = pal;
    if (d->mainFrame)
        d->mainFrame->d->updateBackground();
}

QPalette QWebPage::palette() const
{
    return d->palette;
}

void QWebPagePrivate::inputMethodEvent(QInputMethodEvent *ev)
{
    WebCore::Frame *frame = page->focusController()->focusedOrMainFrame();
    WebCore::Editor *editor = frame->editor();

    if (!editor->canEdit()) {
        ev->ignore();
        return;
    }

    if (!ev->preeditString().isEmpty()) {        
        QString preedit = ev->preeditString();
        // ### FIXME: use the provided QTextCharFormat (use color at least)
        Vector<CompositionUnderline> underlines;
        underlines.append(CompositionUnderline(0, preedit.length(), Color(0,0,0), false));
        editor->setComposition(preedit, underlines, preedit.length(), 0);
    } else if (!ev->commitString().isEmpty()) {
        editor->confirmComposition(ev->commitString());
    }
    ev->accept();
}

void QWebPagePrivate::shortcutOverrideEvent(QKeyEvent* event)
{
    WebCore::Frame* frame = page->focusController()->focusedOrMainFrame();
    WebCore::Editor* editor = frame->editor();
    if (editor->canEdit()) {
        if (event->modifiers() == Qt::NoModifier
            || event->modifiers() == Qt::ShiftModifier
            || event->modifiers() == Qt::KeypadModifier) {
                if (event->key() < Qt::Key_Escape) {
                    event->accept();
                } else {
                    switch (event->key()) {
                    case Qt::Key_Return:
                    case Qt::Key_Enter:
                    case Qt::Key_Delete:
                    case Qt::Key_Home:
                    case Qt::Key_End:
                    case Qt::Key_Backspace:
                    case Qt::Key_Left:
                    case Qt::Key_Right:
                    case Qt::Key_Up:
                    case Qt::Key_Down:
                    case Qt::Key_Tab:
                        event->accept();
                    default:
                        break;
                    }
                }
        }
#ifndef QT_NO_SHORTCUT
        else if (editorActionForKeyEvent(event) != QWebPage::NoWebAction) {
            event->accept();
        }
#endif
    }
}

/*!
  This method is used by the input method to query a set of properties of the page
  to be able to support complex input method operations as support for surrounding
  text and reconversions.

  \a property specifies which property is queried.

  \sa QWidget::inputMethodEvent(), QInputMethodEvent, QInputContext
*/
QVariant QWebPage::inputMethodQuery(Qt::InputMethodQuery property) const
{
    switch(property) {
    case Qt::ImMicroFocus: {
        Frame *frame = d->page->focusController()->focusedFrame();
        if (frame) {
            return QVariant(frame->selectionController()->caretRect());
        }
        return QVariant();
    }
    case Qt::ImFont: {
        QWebView *webView = qobject_cast<QWebView *>(d->view);
        if (webView)
            return QVariant(webView->font());
        return QVariant();
    }
    case Qt::ImCursorPosition: {
        Frame *frame = d->page->focusController()->focusedFrame();
        if (frame) {
            Selection selection = frame->selectionController()->selection();
            if (selection.isCaret()) {
                return QVariant(selection.start().offset());
            }
        }
        return QVariant();
    }
    case Qt::ImSurroundingText: {
        Frame *frame = d->page->focusController()->focusedFrame();
        if (frame) {
            Document *document = frame->document();
            if (document->focusedNode()) {
                return QVariant(document->focusedNode()->nodeValue());
            }
        }
        return QVariant();
    }
    case Qt::ImCurrentSelection:
        return QVariant(selectedText());
    default:
        return QVariant();
    }
}

/*!
   \enum QWebPage::FindFlag

   This enum describes the options available to QWebPage's findText() function. The options
   can be OR-ed together from the following list:

   \value FindBackward Searches backwards instead of forwards.
   \value FindCaseSensitively By default findText() works case insensitive. Specifying this option
   changes the behaviour to a case sensitive find operation.
   \value FindWrapsAroundDocument Makes findText() restart from the beginning of the document if the end
   was reached and the text was not found.
*/

/*!
    \enum QWebPage::LinkDelegationPolicy

    This enum defines the delegation policies a webpage can have when activating links and emitting
    the linkClicked() signal.

    \value DontDelegateLinks No links are delegated. Instead, QWebPage tries to handle them all.
    \value DelegateExternalLinks When activating links that point to documents not stored on the
    local filesystem or an equivalent - such as the Qt resource system - then linkClicked() is emitted.
    \value DelegateAllLinks Whenever a link is activated the linkClicked() signal is emitted.
*/

/*!
    \enum QWebPage::NavigationType

    This enum describes the types of navigation available when browsing through hyperlinked
    documents.

    \value NavigationTypeLinkClicked The user clicked on a link or pressed return on a focused link.
    \value NavigationTypeFormSubmitted The user activated a submit button for an HTML form.
    \value NavigationTypeBackOrForward Navigation to a previously shown document in the back or forward history is requested.
    \value NavigationTypeReload The user activated the reload action.
    \value NavigationTypeFormResubmitted An HTML form was submitted a second time.
    \value NavigationTypeOther A navigation to another document using a method not listed above.
*/

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
    \value Back Navigate back in the history of navigated links.
    \value Forward Navigate forward in the history of navigated links.
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
    \enum QWebPage::WebWindowType

    \value WebBrowserWindow The window is a regular web browser window.
    \value WebModalDialog The window acts as modal dialog.
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

    connect(this, SIGNAL(loadProgress(int)), this, SLOT(_q_onLoadProgressChanged(int)));
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
    This function is called whenever a JavaScript program tries to print a \a message to the web browser's console.

    For example in case of evaluation errors the source URL may be provided in \a sourceID as well as the \a lineNumber.

    The default implementation prints nothing.
*/
void QWebPage::javaScriptConsoleMessage(const QString& message, int lineNumber, const QString& sourceID)
{
    Q_UNUSED(message)
    Q_UNUSED(lineNumber)
    Q_UNUSED(sourceID)
}

/*!
    This function is called whenever a JavaScript program running inside \a frame calls the alert() function with
    the message \a msg.

    The default implementation shows the message, \a msg, with QMessageBox::information.
*/
void QWebPage::javaScriptAlert(QWebFrame *frame, const QString& msg)
{
    QMessageBox::information(d->view, mainFrame()->title(), msg, QMessageBox::Ok);
}

/*!
    This function is called whenever a JavaScript program running inside \a frame calls the confirm() function
    with the message, \a msg. Returns true if the user confirms the message; otherwise returns false.

    The default implementation executes the query using QMessageBox::information with QMessageBox::Yes and QMessageBox::No buttons.
*/
bool QWebPage::javaScriptConfirm(QWebFrame *frame, const QString& msg)
{
    return QMessageBox::Yes == QMessageBox::information(d->view, mainFrame()->title(), msg, QMessageBox::Yes, QMessageBox::No);
}

/*!
    This function is called whenever a JavaScript program running inside \a frame tries to prompt the user for input.
    The program may provide an optional message, \a msg, as well as a default value for the input in \a defaultValue.

    If the prompt was cancelled by the user the implementation should return false; otherwise the
    result should be written to \a result and true should be returned.

    The default implementation uses QInputDialog::getText.
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
    This function is called whenever WebKit wants to create a new window of the given \a type, for
    example when a JavaScript program requests to open a document in a new window.

    If the new window can be created, the new window's QWebPage is returned; otherwise a null pointer is returned.

    If the view associated with the web page is a QWebView object, then the default implementation forwards
    the request to QWebView's createWindow() function; otherwise it returns a null pointer.
*/
QWebPage *QWebPage::createWindow(WebWindowType type)
{
    QWebView *webView = qobject_cast<QWebView *>(d->view);
    if (webView) {
        QWebView *newView = webView->createWindow(type);
        if (newView)
            return newView->page();
    }
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
    if (!frame)
        return;
    WebCore::Editor *editor = frame->editor();
    const char *command = 0;

    switch (action) {
        case OpenLink:
            if (QWebFrame *targetFrame = d->hitTestResult.linkTargetFrame()) {
                WTF::RefPtr<WebCore::Frame> wcFrame = targetFrame->d->frame;
                targetFrame->d->frame->loader()->load(frameLoadRequest(d->hitTestResult.linkUrl(), wcFrame.get()),
                                                      /*lockHistory*/ false,
                                                      /*userGesture*/ true,
                                                      /*event*/ 0,
                                                      /*HTMLFormElement*/ 0,
                                                      /*formValues*/
                                                      WTF::HashMap<String, String>());
                break;
            }
            // fall through
        case OpenLinkInNewWindow:
            openNewWindow(d->hitTestResult.linkUrl(), frame);
            break;
        case OpenFrameInNewWindow: {
            KURL url = frame->loader()->documentLoader()->unreachableURL();
            if (url.isEmpty())
                url = frame->loader()->documentLoader()->url();
            openNewWindow(url, frame);
            break;
        }
        case CopyLinkToClipboard:
            editor->copyURL(d->hitTestResult.linkUrl(), d->hitTestResult.linkText());
            break;
        case OpenImageInNewWindow:
            openNewWindow(d->hitTestResult.imageUrl(), frame);
            break;
        case DownloadImageToDisk:
            frame->loader()->client()->startDownload(WebCore::ResourceRequest(d->hitTestResult.imageUrl(), frame->loader()->outgoingReferrer()));
            break;
        case DownloadLinkToDisk:
            frame->loader()->client()->startDownload(WebCore::ResourceRequest(d->hitTestResult.linkUrl(), frame->loader()->outgoingReferrer()));
            break;
        case CopyImageToClipboard:
            QApplication::clipboard()->setPixmap(d->hitTestResult.pixmap());
            break;
        case Back:
            d->page->goBack();
            break;
        case Forward:
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
            break;

        case InspectElement:
            d->page->inspectorController()->inspect(d->hitTestResult.d->innerNonSharedNode.get());
            break;

        default: break;
    }

    if (command)
        editor->command(command).execute();
}

QSize QWebPage::viewportSize() const
{
    if (d->mainFrame && d->mainFrame->d->frame->view())
        return d->mainFrame->d->frame->view()->frameGeometry().size();

    return d->viewportSize;
}

/*!
    \property QWebPage::viewportSize
    \brief the size of the viewport

    The size affects for example the visibility of scrollbars
    if the document is larger than the viewport.
*/
void QWebPage::setViewportSize(const QSize &size) const
{
    d->viewportSize = size;

    QWebFrame *frame = mainFrame();
    if (frame->d->frame && frame->d->frame->view()) {
        WebCore::FrameView* view = frame->d->frame->view();
        view->setFrameGeometry(QRect(QPoint(0, 0), size));
        frame->d->frame->forceLayout();
        view->adjustViewSize();
    }
}


/*!
    \fn bool QWebPage::acceptNavigationRequest(QWebFrame *frame, const QWebNetworkRequest &request, QWebPage::NavigationType type)

    This function is called whenever WebKit requests to navigate \a frame to the resource specified by \a request by means of
    the specified navigation type \a type.

    The default implementation interprets the page's linkDelegationPolicy and emits linkClicked accordingly or returns true
    to let QWebPage handle the navigation itself.
*/
#if QT_VERSION < 0x040400
bool QWebPage::acceptNavigationRequest(QWebFrame *frame, const QWebNetworkRequest &request, QWebPage::NavigationType type)
#else
bool QWebPage::acceptNavigationRequest(QWebFrame *frame, const QNetworkRequest &request, QWebPage::NavigationType type)
#endif
{
    if (type == NavigationTypeLinkClicked) {
        switch (d->linkPolicy) {
            case DontDelegateLinks:
                return true;

            case DelegateExternalLinks:
                if (WebCore::FrameLoader::shouldTreatSchemeAsLocal(request.url().scheme()))
                    return true;
                emit linkClicked(request.url());
                return false;

            case DelegateAllLinks:
                emit linkClicked(request.url());
                return false;
        }
    }
    return true;
}

/*!
    \property QWebPage::selectedText
    \brief the text currently selected
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
    QIcon icon;
    QStyle *style = view() ? view()->style() : qApp->style();
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

        case Back:
            text = contextMenuItemTagGoBack();
#if QT_VERSION >= 0x040400
            icon = style->standardIcon(QStyle::SP_ArrowBack);
#endif
            break;
        case Forward:
            text = contextMenuItemTagGoForward();
#if QT_VERSION >= 0x040400
            icon = style->standardIcon(QStyle::SP_ArrowForward);
#endif
            break;
        case Stop:
            text = contextMenuItemTagStop();
#if QT_VERSION >= 0x040400
            icon = style->standardIcon(QStyle::SP_BrowserStop);
#endif
            break;
        case Reload:
            text = contextMenuItemTagReload();
#if QT_VERSION >= 0x040400
            icon = style->standardIcon(QStyle::SP_BrowserReload);
#endif
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
    a->setIcon(icon);

    connect(a, SIGNAL(triggered(bool)),
            this, SLOT(_q_webActionTriggered(bool)));

    d->actions[action] = a;
    d->updateAction(action);
    return a;
}

/*!
    \property QWebPage::modified
    \brief whether the page contains unsubmitted form data
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
    case QEvent::Timer:    
        d->timerEvent(static_cast<QTimerEvent*>(ev));
        break;
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
    case QEvent::InputMethod:
        d->inputMethodEvent(static_cast<QInputMethodEvent*>(ev));
    case QEvent::ShortcutOverride:
        d->shortcutOverrideEvent(static_cast<QKeyEvent*>(ev));
        break;
    case QEvent::Leave:
        d->leaveEvent(ev);
        break;
    default:
        return QObject::event(ev);
    }

    return true;
}

/*!
    Similar to QWidget::focusNextPrevChild it focuses the next focusable web element
    if \a next is true; otherwise the previous element is focused.

    Returns true if it can find a new focusable element, or false if it can't.
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
    \property QWebPage::forwardUnsupportedContent
    \brief whether QWebPage should forward unsupported content through the
    unsupportedContent signal

    If disabled the download of such content is aborted immediately.

    By default unsupported content is not forwarded.
*/

void QWebPage::setForwardUnsupportedContent(bool forward)
{
    d->forwardUnsupportedContent = forward;
}

bool QWebPage::forwardUnsupportedContent() const
{
    return d->forwardUnsupportedContent;
}

/*!
    \property QWebPage::linkDelegationPolicy
    \brief how QWebPage should delegate the handling of links through the
    linkClicked() signal

    The default is to delegate no links.
*/

void QWebPage::setLinkDelegationPolicy(LinkDelegationPolicy policy)
{
    d->linkPolicy = policy;
}

QWebPage::LinkDelegationPolicy QWebPage::linkDelegationPolicy() const
{
    return d->linkPolicy;
}

/*!
    Passes the context menu event, \a event, to the currently displayed webpage and
    returns true if the page handled the event; otherwise false is returned.

    A web page may swallow a context menu event through a custom event handler, allowing for context
    menus to be implemented in HTML/JavaScript. This is used by \l{http://maps.google.com/}{Google
    Maps}, for example.
*/
bool QWebPage::swallowContextMenuEvent(QContextMenuEvent *event)
{
    d->page->contextMenuController()->clearContextMenu();

    WebCore::Frame* focusedFrame = d->page->focusController()->focusedOrMainFrame();
    focusedFrame->eventHandler()->sendContextMenuEvent(PlatformMouseEvent(event, 1));
    ContextMenu *menu = d->page->contextMenuController()->contextMenu();
    // If the website defines its own handler then sendContextMenuEvent takes care of
    // calling/showing it and the context menu pointer will be zero. This is the case
    // on maps.google.com for example.
    return !menu;
}

/*!
    Updates the page's actions depending on the position \a pos. For example if \a pos is over an image
    element the CopyImageToClipboard action is enabled.
*/
void QWebPage::updatePositionDependentActions(const QPoint &pos)
{
    // disable position dependent actions first and enable them if WebCore adds them enabled to the context menu.

    for (int i = ContextMenuItemTagNoAction; i < ContextMenuItemBaseApplicationTag; ++i) {
        QWebPage::WebAction action = webActionForContextMenuAction(WebCore::ContextMenuAction(i));
        QAction *a = this->action(action);
        if (a)
            a->setEnabled(false);
    }

    WebCore::Frame* focusedFrame = d->page->focusController()->focusedOrMainFrame();
    HitTestResult result = focusedFrame->eventHandler()->hitTestResultAtPoint(focusedFrame->view()->windowToContents(pos), /*allowShadowContent*/ false);

    d->hitTestResult = QWebHitTestResult(new QWebHitTestResultPrivate(result));
    WebCore::ContextMenu menu(result);
    menu.populate();
    if (d->page->inspectorController()->enabled())
        menu.addInspectElementItem();

    delete d->currentContextMenu;
    // createContextMenu also enables actions if necessary
    d->currentContextMenu = d->createContextMenu(&menu, menu.platformDescription());
}

/*!
    \enum QWebPage::Extension

    This enum describes the types of extensions that the page can support. Before using these extensions, you
    should verify that the extension is supported by calling supportsExtension().

    Currently there are no extensions.
*/

/*!
    \class QWebPage::ExtensionOption
    \since 4.4
    \brief The ExtensionOption class provides an extended input argument to QWebPage's extension support.

    \sa QWebPage::extension()
*/

/*!
    \class QWebPage::ExtensionReturn
    \since 4.4
    \brief The ExtensionOption class provides an extended output argument to QWebPage's extension support.

    \sa QWebPage::extension()
*/

/*!
    This virtual function can be reimplemented in a QWebPage subclass to provide support for extensions. The \a option
    argument is provided as input to the extension; the output results can be stored in \a output.

    The behavior of this function is determined by \a extension.

    You can call supportsExtension() to check if an extension is supported by the page.

    By default, no extensions are supported, and this function returns false.

    \sa supportsExtension(), Extension
*/
bool QWebPage::extension(Extension extension, const ExtensionOption *option, ExtensionReturn *output)
{
    Q_UNUSED(extension)
    Q_UNUSED(option)
    Q_UNUSED(output)
    return false;
}

/*!
    This virtual function returns true if the web page supports \a extension; otherwise false is returned.

    \sa extension()
*/
bool QWebPage::supportsExtension(Extension extension) const
{
    Q_UNUSED(extension)
    return false;
}

/*!
    Finds the next occurrence of the string, \a subString, in the page, using the given \a options.
    Returns true of \a subString was found and selects the match visually; otherwise returns false.
*/
bool QWebPage::findText(const QString &subString, FindFlags options)
{
    ::TextCaseSensitivity caseSensitivity = ::TextCaseInsensitive;
    if (options & FindCaseSensitively)
        caseSensitivity = ::TextCaseSensitive;

    ::FindDirection direction = ::FindDirectionForward;
    if (options & FindBackward)
        direction = ::FindDirectionBackward;

    const bool shouldWrap = options & FindWrapsAroundDocument;

    return d->page->findString(subString, caseSensitivity, direction, shouldWrap);
}

/*!
    Returns a pointer to the page's settings object.
*/
QWebSettings *QWebPage::settings() const
{
    return d->settings;
}

/*!
    This function is called when the web content requests a file name, for example
    as a result of the user clicking on a "file upload" button in a HTML form.

    A suggested filename may be provided in \a suggestedFile. The frame originating the
    request is provided as \a parentFrame.
*/
QString QWebPage::chooseFile(QWebFrame *parentFrame, const QString& suggestedFile)
{
#ifndef QT_NO_FILEDIALOG
    return QFileDialog::getOpenFileName(d->view, QString::null, suggestedFile);
#else
    return QString::null;
#endif
}

#if QT_VERSION < 0x040400 && !defined qdoc

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
    Sets the QNetworkAccessManager \a manager responsible for serving network requests for this
    QWebPage.

    \sa networkAccessManager()
*/
void QWebPage::setNetworkAccessManager(QNetworkAccessManager *manager)
{
    if (manager == d->networkManager)
        return;
    if (d->networkManager && d->networkManager->parent() == this)
        delete d->networkManager;
    d->networkManager = manager;
}

/*!
    Returns the QNetworkAccessManager that is responsible for serving network
    requests for this QWebPage.

    \sa setNetworkAccessManager()
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
    Sets the QWebPluginFactory \a factory responsible for creating plugins embedded into this
    QWebPage.

    \sa pluginFactory()
*/
void QWebPage::setPluginFactory(QWebPluginFactory *factory)
{
    d->pluginFactory = factory;
}

/*!
    Returns the QWebPluginFactory that is responsible for creating plugins embedded into
    this QWebPage. If no plugin factory is installed a null pointer is returned.

    \sa setPluginFactory()
*/
QWebPluginFactory *QWebPage::pluginFactory() const
{
    return d->pluginFactory;
}

/*!
    This function is called when a user agent for HTTP requests is needed. You can re-implement this
    function to dynamically return different user agent's for different urls, based on the \a url parameter.

    The default implementation returns the following value:

    "Mozilla/5.0 (%Platform%; %Security%; %Subplatform%; %Locale%) AppleWebKit/%WebKitVersion% (KHTML, like Gecko, Safari/419.3) %AppVersion"

    In this string the following values are replaced at run-time:
    \list
    \o %Platform% and %Subplatform% are expanded to the windowing system and the operation system.
    \o %Security% expands to U if SSL is enabled, otherwise N. SSL is enabled if QSslSocket::supportsSsl() returns true.
    \o %Locale% is replaced with QLocale::name().
    \o %WebKitVersion% currently expands to 523.15
    \o %AppVersion% expands to QCoreApplication::applicationName()/QCoreApplication::applicationVersion() if they're set; otherwise defaulting to Qt and the current Qt version.
    \endlist
*/
QString QWebPage::userAgentForUrl(const QUrl& url) const
{
    Q_UNUSED(url)
    QString ua = QLatin1String("Mozilla/5.0 ("

    // Plastform
#ifdef Q_WS_MAC
    "Macintosh"
#elif defined Q_WS_QWS
    "QtEmbedded"
#elif defined Q_WS_WIN
    "Windows"
#elif defined Q_WS_X11
    "X11"
#else
    "Unknown"
#endif
    "; "

    // Placeholder for security strength (N or U)
    "%1; "

    // Subplatform"
#ifdef Q_OS_AIX
    "AIX"
#elif defined Q_OS_WIN32
    "%2"
#elif defined Q_OS_DARWIN
#ifdef __i386__ || __x86_64__
    "Intel Mac OS X"
#else
    "PPC Mac OS X"
#endif

#elif defined Q_OS_BSDI
    "BSD"
#elif defined Q_OS_BSD4
    "BSD Four"
#elif defined Q_OS_CYGWIN
    "Cygwin"
#elif defined Q_OS_DGUX
    "DG/UX"
#elif defined Q_OS_DYNIX
    "DYNIX/ptx"
#elif defined Q_OS_FREEBSD
    "FreeBSD"
#elif defined Q_OS_HPUX
    "HP-UX"
#elif defined Q_OS_HURD
    "GNU Hurd"
#elif defined Q_OS_IRIX
    "SGI Irix"
#elif defined Q_OS_LINUX
    "Linux"
#elif defined Q_OS_LYNX
    "LynxOS"
#elif defined Q_OS_NETBSD
    "NetBSD"
#elif defined Q_OS_OS2
    "OS/2"
#elif defined Q_OS_OPENBSD
    "OpenBSD"
#elif defined Q_OS_OS2EMX
    "OS/2"
#elif defined Q_OS_OSF
    "HP Tru64 UNIX"
#elif defined Q_OS_QNX6
    "QNX RTP Six"
#elif defined Q_OS_QNX
    "QNX"
#elif defined Q_OS_RELIANT
    "Reliant UNIX"
#elif defined Q_OS_SCO
    "SCO OpenServer"
#elif defined Q_OS_SOLARIS
    "Sun Solaris"
#elif defined Q_OS_ULTRIX
    "DEC Ultrix"
#elif defined Q_OS_UNIX
    "UNIX BSD/SYSV system"
#elif defined Q_OS_UNIXWARE
    "UnixWare Seven, Open UNIX Eight"
#else
    "Unknown"
#endif
    "; ");

    QChar securityStrength(QLatin1Char('N'));
#if !defined(QT_NO_OPENSSL)
    if (QSslSocket::supportsSsl())
        securityStrength = QLatin1Char('U');
#endif
    ua = ua.arg(securityStrength);

#if defined Q_OS_WIN32
    QString ver;
    switch(QSysInfo::WindowsVersion) {
        case QSysInfo::WV_32s:
            ver = "Windows 3.1";
            break;
        case QSysInfo::WV_95:
            ver = "Windows 95";
            break;
        case QSysInfo::WV_98:
            ver = "Windows 98";
            break;
        case QSysInfo::WV_Me:
            ver = "Windows 98; Win 9x 4.90";
            break;
        case QSysInfo::WV_NT:
            ver = "WinNT4.0";
            break;
        case QSysInfo::WV_2000:
            ver = "Windows NT 5.0";
            break;
        case QSysInfo::WV_XP:
            ver = "Windows NT 5.1";
            break;
        case QSysInfo::WV_2003:
            ver = "Windows NT 5.2";
            break;
        case QSysInfo::WV_VISTA:
            ver = "Windows NT 6.0";
            break;
        case QSysInfo::WV_CE:
            ver = "Windows CE";
            break;
        case QSysInfo::WV_CENET:
            ver = "Windows CE .NET";
            break;
        case QSysInfo::WV_CE_5:
            ver = "Windows CE 5.x";
            break;
        case QSysInfo::WV_CE_6:
            ver = "Windows CE 6.x";
            break;
    }
    ua = QString(ua).arg(ver);
#endif

    // Language
    QLocale locale;
    if (d->view)
        locale = d->view->locale();
    QString name = locale.name();
    name[2] = QLatin1Char('-');
    ua.append(name);
    ua.append(QLatin1String(") "));

    // webkit/qt version
    ua.append(QLatin1String("AppleWebKit/" WEBKIT_VERSION " (KHTML, like Gecko, Safari/419.3) "));

    // Application name/version
    QString appName = QCoreApplication::applicationName();
    if (!appName.isEmpty()) {
        ua.append(QLatin1Char(' ') + appName);
#if QT_VERSION >= 0x040400
        QString appVer = QCoreApplication::applicationVersion();
        if (!appVer.isEmpty())
            ua.append(QLatin1Char('/') + appVer);
#endif
    } else {
        // Qt version
        ua.append(QLatin1String("Qt/"));
        ua.append(QLatin1String(qVersion()));
    }
    return ua;
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

/*!
    \fn void QWebPage::loadProgress(int progress)

    This signal is emitted when the global progress status changes.
    The current value is provided by \a progress and scales from 0 to 100,
    which is the default range of QProgressBar.
    It accumulates changes from all the child frames.
*/

/*!
    \fn void QWebPage::linkHovered(const QString &link, const QString &title, const QString &textContent)

    This signal is emitted when the mouse is hovering over a link.
    The first parameter is the \a link url, the second is the link \a title
    if any, and third \a textContent is the text content. Method is emitter with both
    empty parameters when the mouse isn't hovering over any link element.
*/

/*!
    \fn void QWebPage::statusBarMessage(const QString& text)

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
    \fn void QWebPage::geometryChangeRequested(const QRect& geom)

    This signal is emitted whenever the document wants to change the position and size of the
    page to \a geom. This can happen for example through JavaScript.
*/

/*!
    \fn void QWebPage::repaintRequested(const QRect& dirtyRect)

    This signal is emitted whenever this QWebPage should be updated and no view was set.
    \a dirtyRect contains the area that needs to be updated. To paint the QWebPage get
    the mainFrame() and call the render(QPainter*, const QRegion&) method with the
    \a dirtyRect as the second parameter.

    \sa mainFrame()
    \sa view()
*/

/*!
    \fn void QWebPage::scrollRequested(int dx, int dy, const QRect& rectToScroll)

    This signal is emitted whenever the content given by \a rectToScroll needs
    to be scrolled \a dx and \a dy downwards and no view was set.

    \sa view()
*/

/*!
    \fn void QWebPage::windowCloseRequested()

    This signal is emitted whenever the page requests the web browser window to be closed,
    for example through the JavaScript \c{window.close()} call.
*/

/*!
    \fn void QWebPage::printRequested(QWebFrame *frame)

    This signal is emitted whenever the page requests the web browser to print \a frame,
    for example through the JavaScript \c{window.print()} call.
*/

/*!
    \fn void QWebPage::unsupportedContent(QNetworkReply *reply)

    This signals is emitted when webkit cannot handle a link the user navigated to.

    At signal emissions time the meta data of the QNetworkReply \a reply is available.

    \note This signal is only emitted if the forwardUnsupportedContent property is set to true.
*/

/*!
    \fn void QWebPage::downloadRequested(const QNetworkRequest &request)

    This signal is emitted when the user decides to download a link. The url of
    the link as well as additional meta-information is contained in \a request.
*/

/*!
    \fn void QWebPage::microFocusChanged()

    This signal is emitted when for example the position of the cursor in an editable form
    element changes. It is used inform input methods about the new on-screen position where
    the user is able to enter text. This signal is usually connected to QWidget's updateMicroFocus()
    slot.
*/

/*!
    \fn void QWebPage::linkClicked(const QUrl &url)

    This signal is emitted whenever the user clicks on a link and the page's linkDelegationPolicy
    property is set to delegate the link handling for the specified \a url.

    By default no links are delegated and are handled by QWebPage instead.
*/

/*!
    \fn void QWebPage::toolBarVisibilityChangeRequested(bool visible)

    This signal is emitted whenever the visibility of the toolbar in a web browser
    window that hosts QWebPage should be changed to \a visible.
*/

/*!
    \fn void QWebPage::statusBarVisibilityChangeRequested(bool visible)

    This signal is emitted whenever the visibility of the statusbar in a web browser
    window that hosts QWebPage should be changed to \a visible.
*/

/*!
    \fn void QWebPage::menuBarVisibilityChangeRequested(bool visible)

    This signal is emitted whenever the visibility of the menubar in a web browser
    window that hosts QWebPage should be changed to \a visible.
*/

#include "moc_qwebpage.cpp"
