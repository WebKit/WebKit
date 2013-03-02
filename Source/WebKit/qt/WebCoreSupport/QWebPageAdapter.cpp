/*
 * Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "QWebPageAdapter.h"

#include "CSSComputedStyleDeclaration.h"
#include "CSSParser.h"
#include "Chrome.h"
#include "ChromeClientQt.h"
#include "ClientRect.h"
#include "ContextMenu.h"
#include "ContextMenuClientQt.h"
#include "ContextMenuController.h"
#if ENABLE(DEVICE_ORIENTATION)
#include "DeviceMotionClientQt.h"
#include "DeviceOrientationClientMock.h"
#include "DeviceOrientationClientQt.h"
#endif
#include "DocumentLoader.h"
#include "DragClientQt.h"
#include "DragController.h"
#include "DragData.h"
#include "DragSession.h"
#include "EditorClientQt.h"
#include "FocusController.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#if ENABLE(GEOLOCATION)
#include "GeolocationClientMock.h"
#include "GeolocationClientQt.h"
#include "GeolocationController.h"
#endif
#include "GeolocationPermissionClientQt.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLInputElement.h"
#include "HitTestResult.h"
#include "InitWebCoreQt.h"
#include "InspectorClientQt.h"
#include "InspectorController.h"
#include "InspectorServerQt.h"
#include "LocalizedStrings.h"
#include "MIMETypeRegistry.h"
#include "MemoryCache.h"
#include "NetworkingContext.h"
#include "NodeList.h"
#include "NotificationPresenterClientQt.h"
#include "PageGroup.h"
#include "Pasteboard.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformTouchEvent.h"
#include "PlatformWheelEvent.h"
#include "PluginDatabase.h"
#include "PluginPackage.h"
#include "ProgressTracker.h"
#include "QWebFrameAdapter.h"
#include "RenderTextControl.h"
#include "SchemeRegistry.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"
#include "Settings.h"
#include "UndoStepQt.h"
#include "UserAgentQt.h"
#include "WebEventConversion.h"
#include "WebKitVersion.h"
#include "WindowFeatures.h"
#include "qwebhistory_p.h"
#include "qwebpluginfactory.h"
#include "qwebsettings.h"
#include <Page.h>
#include <QBitArray>
#include <QGuiApplication>
#include <QMimeData>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QStyleHints>
#include <QTextCharFormat>
#include <QTouchEvent>
#include <QWheelEvent>

// from text/qfont.cpp
QT_BEGIN_NAMESPACE
extern Q_GUI_EXPORT int qt_defaultDpi();
QT_END_NAMESPACE

using namespace WebCore;

bool QWebPageAdapter::drtRun = false;

typedef QWebPageAdapter::MenuItemDescription MenuItem;

static inline DragOperation dropActionToDragOp(Qt::DropActions actions)
{
    unsigned result = 0;
    if (actions & Qt::CopyAction)
        result |= DragOperationCopy;
    // DragOperationgeneric represents InternetExplorer's equivalent of Move operation,
    // hence it should be considered as "move"
    if (actions & Qt::MoveAction)
        result |= (DragOperationMove | DragOperationGeneric);
    if (actions & Qt::LinkAction)
        result |= DragOperationLink;
    if (result == (DragOperationCopy | DragOperationMove | DragOperationGeneric | DragOperationLink))
        result = DragOperationEvery;
    return (DragOperation)result;
}

static inline Qt::DropAction dragOpToDropAction(unsigned actions)
{
    Qt::DropAction result = Qt::IgnoreAction;
    if (actions & DragOperationCopy)
        result = Qt::CopyAction;
    else if (actions & DragOperationMove)
        result = Qt::MoveAction;
    // DragOperationgeneric represents InternetExplorer's equivalent of Move operation,
    // hence it should be considered as "move"
    else if (actions & DragOperationGeneric)
        result = Qt::MoveAction;
    else if (actions & DragOperationLink)
        result = Qt::LinkAction;
    return result;
}

static WebCore::FrameLoadRequest frameLoadRequest(const QUrl &url, WebCore::Frame *frame)
{
    return WebCore::FrameLoadRequest(frame->document()->securityOrigin(),
        WebCore::ResourceRequest(url, frame->loader()->outgoingReferrer()));
}

static void openNewWindow(const QUrl& url, Frame* frame)
{
    if (Page* oldPage = frame->page()) {
        WindowFeatures features;
        NavigationAction action;
        FrameLoadRequest request = frameLoadRequest(url, frame);
        if (Page* newPage = oldPage->chrome()->createWindow(frame, request, features, action)) {
            newPage->mainFrame()->loader()->loadFrameRequest(request, false, false, 0, 0, MaybeSendReferrer);
            newPage->chrome()->show();
        }
    }
}

QWebPageAdapter::QWebPageAdapter()
    : settings(0)
    , page(0)
    , pluginFactory(0)
    , forwardUnsupportedContent(false)
    , insideOpenCall(false)
    , clickCausedFocus(false)
    , m_totalBytes(0)
    , m_bytesReceived()
    , networkManager(0)
{
    WebCore::initializeWebCoreQt();
}

void QWebPageAdapter::initializeWebCorePage()
{
#if ENABLE(GEOLOCATION) || ENABLE(DEVICE_ORIENTATION)
    bool useMock = QWebPageAdapter::drtRun;
#endif
    Page::PageClients pageClients;
    pageClients.chromeClient = new ChromeClientQt(this);
    pageClients.contextMenuClient = new ContextMenuClientQt();
    pageClients.editorClient = new EditorClientQt(this);
    pageClients.dragClient = new DragClientQt(pageClients.chromeClient);
    pageClients.inspectorClient = new InspectorClientQt(this);
    page = new Page(pageClients);
#if ENABLE(GEOLOCATION)
    if (useMock) {
        // In case running in DumpRenderTree mode set the controller to mock provider.
        GeolocationClientMock* mock = new GeolocationClientMock;
        WebCore::provideGeolocationTo(page, mock);
        mock->setController(WebCore::GeolocationController::from(page));
    } else
        WebCore::provideGeolocationTo(page, new GeolocationClientQt(this));
#endif
#if ENABLE(DEVICE_ORIENTATION)
    if (useMock)
        WebCore::provideDeviceOrientationTo(page, new DeviceOrientationClientMock);
    else
        WebCore::provideDeviceOrientationTo(page, new DeviceOrientationClientQt);
    WebCore::provideDeviceMotionTo(page, new DeviceMotionClientQt);
#endif

    // By default each page is put into their own unique page group, which affects popup windows
    // and visited links. Page groups (per process only) is a feature making it possible to use
    // separate settings for each group, so that for instance an integrated browser/email reader
    // can use different settings for displaying HTML pages and HTML email. To make QtWebKit work
    // as expected out of the box, we use a default group similar to what other ports are doing.
    page->setGroupName("Default Group");

    page->addLayoutMilestones(DidFirstVisuallyNonEmptyLayout);

    settings = new QWebSettings(page->settings());

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    WebCore::provideNotification(page, NotificationPresenterClientQt::notificationPresenter());
#endif

    history.d = new QWebHistoryPrivate(static_cast<WebCore::BackForwardListImpl*>(page->backForwardList()));

    PageGroup::setShouldTrackVisitedLinks(true);
}

QWebPageAdapter::~QWebPageAdapter()
{
    delete page;
    delete settings;

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    NotificationPresenterClientQt::notificationPresenter()->removeClient();
#endif
}

void QWebPageAdapter::deletePage()
{
    // Before we delete the page, detach the mainframe's loader
    FrameLoader* loader = mainFrameAdapter()->frame->loader();
    if (loader)
        loader->detachFromParent();
    delete page;
    page = 0;
}

QWebPageAdapter* QWebPageAdapter::kit(Page* page)
{
    return static_cast<ChromeClientQt*>(page->chrome()->client())->m_webPage;
}

ViewportArguments QWebPageAdapter::viewportArguments() const
{
    return page ? page->viewportArguments() : WebCore::ViewportArguments();
}


void QWebPageAdapter::registerUndoStep(WTF::PassRefPtr<WebCore::UndoStep> step)
{
    createUndoStep(QSharedPointer<UndoStepQt>(new UndoStepQt(step)));
}

void QWebPageAdapter::setNetworkAccessManager(QNetworkAccessManager *manager)
{
    if (manager == networkManager)
        return;
    if (networkManager && networkManager->parent() == handle())
        delete networkManager;
    networkManager = manager;
}

QNetworkAccessManager* QWebPageAdapter::networkAccessManager()
{
    if (!networkManager)
        networkManager = new QNetworkAccessManager(handle());
    return networkManager;
}

bool QWebPageAdapter::hasSelection() const
{
    Frame* frame = page->focusController()->focusedOrMainFrame();
    if (frame)
        return (frame->selection()->selection().selectionType() != VisibleSelection::NoSelection);
    return false;
}

QString QWebPageAdapter::selectedText() const
{
    Frame* frame = page->focusController()->focusedOrMainFrame();
    if (frame->selection()->selection().selectionType() == VisibleSelection::NoSelection)
        return QString();
    return frame->editor()->selectedText();
}

QString QWebPageAdapter::selectedHtml() const
{
    return page->focusController()->focusedOrMainFrame()->editor()->selectedRange()->toHTML();
}

bool QWebPageAdapter::isContentEditable() const
{
    return page->isEditable();
}

void QWebPageAdapter::setContentEditable(bool editable)
{
    page->setEditable(editable);
    page->setTabKeyCyclesThroughElements(!editable);

    Frame* frame = mainFrameAdapter()->frame;
    if (editable) {
        frame->editor()->applyEditingStyleToBodyElement();
        // FIXME: mac port calls this if there is no selectedDOMRange
        // frame->setSelectionFromNone();
    }

}

bool QWebPageAdapter::findText(const QString& subString, FindFlag options)
{
    ::TextCaseSensitivity caseSensitivity = ::TextCaseInsensitive;
    if (options & FindCaseSensitively)
        caseSensitivity = ::TextCaseSensitive;

    if (options & HighlightAllOccurrences) {
        if (subString.isEmpty()) {
            page->unmarkAllTextMatches();
            return true;
        }
        return page->markAllMatchesForText(subString, caseSensitivity, true, 0);
    }

    if (subString.isEmpty()) {
        page->mainFrame()->selection()->clear();
        Frame* frame = page->mainFrame()->tree()->firstChild();
        while (frame) {
            frame->selection()->clear();
            frame = frame->tree()->traverseNextWithWrap(false);
        }
    }
    ::FindDirection direction = ::FindDirectionForward;
    if (options & FindBackward)
        direction = ::FindDirectionBackward;

    const bool shouldWrap = options & FindWrapsAroundDocument;

    return page->findString(subString, caseSensitivity, direction, shouldWrap);
}

void QWebPageAdapter::adjustPointForClicking(QMouseEvent* ev)
{
#if ENABLE(TOUCH_ADJUSTMENT)
    QtPlatformPlugin platformPlugin;
    OwnPtr<QWebTouchModifier> touchModifier = platformPlugin.createTouchModifier();
    if (!touchModifier)
        return;

    unsigned topPadding = touchModifier->hitTestPaddingForTouch(QWebTouchModifier::Up);
    unsigned rightPadding = touchModifier->hitTestPaddingForTouch(QWebTouchModifier::Right);
    unsigned bottomPadding = touchModifier->hitTestPaddingForTouch(QWebTouchModifier::Down);
    unsigned leftPadding = touchModifier->hitTestPaddingForTouch(QWebTouchModifier::Left);

    touchModifier = nullptr;

    if (!topPadding && !rightPadding && !bottomPadding && !leftPadding)
        return;

    EventHandler* eventHandler = page->mainFrame()->eventHandler();
    ASSERT(eventHandler);

    IntRect touchRect(ev->pos().x() - leftPadding, ev->pos().y() - topPadding, leftPadding + rightPadding, topPadding + bottomPadding);
    IntPoint adjustedPoint;
    Node* adjustedNode;
    bool foundClickableNode = eventHandler->bestClickableNodeForTouchPoint(touchRect.center(), touchRect.size(), adjustedPoint, adjustedNode);
    if (!foundClickableNode)
        return;

    QMouseEvent* ret = new QMouseEvent(ev->type(), QPoint(adjustedPoint), ev->globalPos(), ev->button(), ev->buttons(), ev->modifiers());
    delete ev;
    ev = ret;
#else
    Q_UNUSED(ev);
#endif
}

void QWebPageAdapter::mouseMoveEvent(QMouseEvent* ev)
{
    WebCore::Frame* frame = mainFrameAdapter()->frame;
    if (!frame->view())
        return;

    bool accepted = frame->eventHandler()->mouseMoved(convertMouseEvent(ev, 0));
    ev->setAccepted(accepted);
}

void QWebPageAdapter::mousePressEvent(QMouseEvent* ev)
{
    WebCore::Frame* frame = mainFrameAdapter()->frame;
    if (!frame->view())
        return;

    RefPtr<WebCore::Node> oldNode;
    Frame* focusedFrame = page->focusController()->focusedFrame();
    if (Document* focusedDocument = focusedFrame ? focusedFrame->document() : 0)
        oldNode = focusedDocument->focusedNode();

    if (tripleClickTimer.isActive()
        && (ev->pos() - tripleClick).manhattanLength() < qGuiApp->styleHints()->startDragDistance()) {
        mouseTripleClickEvent(ev);
        return;
    }

    bool accepted = false;
    PlatformMouseEvent mev = convertMouseEvent(ev, 1);
    // ignore the event if we can't map Qt's mouse buttons to WebCore::MouseButton
    if (mev.button() != NoButton)
        accepted = frame->eventHandler()->handleMousePressEvent(mev);
    ev->setAccepted(accepted);

    RefPtr<WebCore::Node> newNode;
    focusedFrame = page->focusController()->focusedFrame();
    if (Document* focusedDocument = focusedFrame ? focusedFrame->document() : 0)
        newNode = focusedDocument->focusedNode();

    if (newNode && oldNode != newNode)
        clickCausedFocus = true;
}

void QWebPageAdapter::mouseDoubleClickEvent(QMouseEvent *ev)
{
    WebCore::Frame* frame = mainFrameAdapter()->frame;
    if (!frame->view())
        return;

    bool accepted = false;
    PlatformMouseEvent mev = convertMouseEvent(ev, 2);
    // ignore the event if we can't map Qt's mouse buttons to WebCore::MouseButton
    if (mev.button() != NoButton)
        accepted = frame->eventHandler()->handleMousePressEvent(mev);
    ev->setAccepted(accepted);

    tripleClickTimer.start(qGuiApp->styleHints()->mouseDoubleClickInterval(), handle());
    tripleClick = QPointF(ev->pos()).toPoint();
}

void QWebPageAdapter::mouseTripleClickEvent(QMouseEvent *ev)
{
    WebCore::Frame* frame = mainFrameAdapter()->frame;
    if (!frame->view())
        return;

    bool accepted = false;
    PlatformMouseEvent mev = convertMouseEvent(ev, 3);
    // ignore the event if we can't map Qt's mouse buttons to WebCore::MouseButton
    if (mev.button() != NoButton)
        accepted = frame->eventHandler()->handleMousePressEvent(mev);
    ev->setAccepted(accepted);
}

void QWebPageAdapter::mouseReleaseEvent(QMouseEvent *ev)
{
    WebCore::Frame* frame = mainFrameAdapter()->frame;
    if (!frame->view())
        return;

    bool accepted = false;
    PlatformMouseEvent mev = convertMouseEvent(ev, 0);
    // ignore the event if we can't map Qt's mouse buttons to WebCore::MouseButton
    if (mev.button() != NoButton)
        accepted = frame->eventHandler()->handleMouseReleaseEvent(mev);
    ev->setAccepted(accepted);

    handleSoftwareInputPanel(ev->button(), QPointF(ev->pos()).toPoint());
}

void QWebPageAdapter::handleSoftwareInputPanel(Qt::MouseButton button, const QPoint& pos)
{
    Frame* frame = page->focusController()->focusedFrame();
    if (!frame)
        return;

    if (client && client->inputMethodEnabled()
        && frame->document()->focusedNode()
            && button == Qt::LeftButton && qGuiApp->property("autoSipEnabled").toBool()) {
        if (!clickCausedFocus || requestSoftwareInputPanel()) {
            HitTestResult result = frame->eventHandler()->hitTestResultAtPoint(frame->view()->windowToContents(pos));
            if (result.isContentEditable()) {
                QEvent event(QEvent::RequestSoftwareInputPanel);
                QGuiApplication::sendEvent(client->ownerWidget(), &event);
            }
        }
    }

    clickCausedFocus = false;
}

#ifndef QT_NO_WHEELEVENT
void QWebPageAdapter::wheelEvent(QWheelEvent *ev, int wheelScrollLines)
{
    WebCore::Frame* frame = mainFrameAdapter()->frame;
    if (!frame->view())
        return;

    PlatformWheelEvent pev = convertWheelEvent(ev, wheelScrollLines);
    bool accepted = frame->eventHandler()->handleWheelEvent(pev);
    ev->setAccepted(accepted);
}
#endif // QT_NO_WHEELEVENT

#ifndef QT_NO_DRAGANDDROP

Qt::DropAction QWebPageAdapter::dragEntered(const QMimeData *data, const QPoint &pos, Qt::DropActions possibleActions)
{
    DragData dragData(data, pos, QCursor::pos(), dropActionToDragOp(possibleActions));
    return dragOpToDropAction(page->dragController()->dragEntered(&dragData).operation);
}

void QWebPageAdapter::dragLeaveEvent()
{
    DragData dragData(0, IntPoint(), QCursor::pos(), DragOperationNone);
    page->dragController()->dragExited(&dragData);
}

Qt::DropAction QWebPageAdapter::dragUpdated(const QMimeData *data, const QPoint &pos, Qt::DropActions possibleActions)
{
    DragData dragData(data, pos, QCursor::pos(), dropActionToDragOp(possibleActions));
    return dragOpToDropAction(page->dragController()->dragUpdated(&dragData).operation);
}

bool QWebPageAdapter::performDrag(const QMimeData *data, const QPoint &pos, Qt::DropActions possibleActions)
{
    DragData dragData(data, pos, QCursor::pos(), dropActionToDragOp(possibleActions));
    return page->dragController()->performDrag(&dragData);
}

void QWebPageAdapter::inputMethodEvent(QInputMethodEvent *ev)
{
    WebCore::Frame *frame = page->focusController()->focusedOrMainFrame();
    WebCore::Editor *editor = frame->editor();

    if (!editor->canEdit()) {
        ev->ignore();
        return;
    }

    Node* node = 0;
    if (frame->selection()->rootEditableElement())
        node = frame->selection()->rootEditableElement()->deprecatedShadowAncestorNode();

    Vector<CompositionUnderline> underlines;
    bool hasSelection = false;

    for (int i = 0; i < ev->attributes().size(); ++i) {
        const QInputMethodEvent::Attribute& a = ev->attributes().at(i);
        switch (a.type) {
        case QInputMethodEvent::TextFormat: {
            QTextCharFormat textCharFormat = a.value.value<QTextFormat>().toCharFormat();
            QColor qcolor = textCharFormat.underlineColor();
            underlines.append(CompositionUnderline(qMin(a.start, (a.start + a.length)), qMax(a.start, (a.start + a.length)), Color(makeRGBA(qcolor.red(), qcolor.green(), qcolor.blue(), qcolor.alpha())), false));
            break;
        }
        case QInputMethodEvent::Cursor: {
            frame->selection()->setCaretVisible(a.length); // if length is 0 cursor is invisible
            if (a.length > 0) {
                RenderObject* caretRenderer = frame->selection()->caretRenderer();
                if (caretRenderer) {
                    QColor qcolor = a.value.value<QColor>();
                    caretRenderer->style()->setColor(Color(makeRGBA(qcolor.red(), qcolor.green(), qcolor.blue(), qcolor.alpha())));
                }
            }
            break;
        }
        case QInputMethodEvent::Selection: {
            hasSelection = true;
            // A selection in the inputMethodEvent is always reflected in the visible text
            if (node) {
                if (isHTMLTextFormControlElement(node))
                    toHTMLTextFormControlElement(node)->setSelectionRange(qMin(a.start, (a.start + a.length)), qMax(a.start, (a.start + a.length)));
            }

            if (!ev->preeditString().isEmpty())
                editor->setComposition(ev->preeditString(), underlines, qMin(a.start, (a.start + a.length)), qMax(a.start, (a.start + a.length)));
            else {
                // If we are in the middle of a composition, an empty pre-edit string and a selection of zero
                // cancels the current composition
                if (editor->hasComposition() && !(a.start + a.length))
                    editor->setComposition(QString(), underlines, 0, 0);
            }
            break;
        }
        default:
            break;
        }
    }

    if (node && ev->replacementLength() > 0) {
        int cursorPos = frame->selection()->extent().offsetInContainerNode();
        int start = cursorPos + ev->replacementStart();
        if (isHTMLTextFormControlElement(node))
            toHTMLTextFormControlElement(node)->setSelectionRange(start, start + ev->replacementLength());
        // Commit regardless of whether commitString is empty, to get rid of selection.
        editor->confirmComposition(ev->commitString());
    } else if (!ev->commitString().isEmpty()) {
        if (editor->hasComposition())
            editor->confirmComposition(ev->commitString());
        else
            editor->insertText(ev->commitString(), 0);
    } else if (!hasSelection && !ev->preeditString().isEmpty())
        editor->setComposition(ev->preeditString(), underlines, 0, 0);
    else if (ev->preeditString().isEmpty() && editor->hasComposition())
        editor->confirmComposition(String());

    ev->accept();
}

QVariant QWebPageAdapter::inputMethodQuery(Qt::InputMethodQuery property) const
{
    Frame* frame = page->focusController()->focusedFrame();
    if (!frame)
        return QVariant();

    WebCore::Editor* editor = frame->editor();

    RenderObject* renderer = 0;
    RenderTextControl* renderTextControl = 0;

    if (frame->selection()->rootEditableElement())
        renderer = frame->selection()->rootEditableElement()->deprecatedShadowAncestorNode()->renderer();

    if (renderer && renderer->isTextControl())
        renderTextControl = toRenderTextControl(renderer);

    switch (property) {
    case Qt::ImMicroFocus: {
        WebCore::FrameView* view = frame->view();
        if (view && view->needsLayout()) {
            // We can't access absoluteCaretBounds() while the view needs to layout.
            return QVariant();
        }
        return QVariant(view->contentsToWindow(frame->selection()->absoluteCaretBounds()));
    }
    case Qt::ImFont: {
        if (renderTextControl) {
            RenderStyle* renderStyle = renderTextControl->style();
            return QVariant(QFont(renderStyle->font().syntheticFont()));
        }
        return QVariant(QFont());
    }
    case Qt::ImCursorPosition: {
        if (editor->hasComposition())
            return QVariant(frame->selection()->end().offsetInContainerNode());
        return QVariant(frame->selection()->extent().offsetInContainerNode());
    }
    case Qt::ImSurroundingText: {
        if (renderTextControl && renderTextControl->textFormControlElement()) {
            QString text = renderTextControl->textFormControlElement()->value();
            RefPtr<Range> range = editor->compositionRange();
            if (range)
                text.remove(range->startPosition().offsetInContainerNode(), TextIterator::rangeLength(range.get()));
            return QVariant(text);
        }
        return QVariant();
    }
    case Qt::ImCurrentSelection: {
        if (!editor->hasComposition() && renderTextControl && renderTextControl->textFormControlElement()) {
            int start = frame->selection()->start().offsetInContainerNode();
            int end = frame->selection()->end().offsetInContainerNode();
            if (end > start)
                return QVariant(QString(renderTextControl->textFormControlElement()->value()).mid(start, end - start));
        }
        return QVariant();

    }
    case Qt::ImAnchorPosition: {
        if (editor->hasComposition())
            return QVariant(frame->selection()->start().offsetInContainerNode());
        return QVariant(frame->selection()->base().offsetInContainerNode());
    }
    case Qt::ImMaximumTextLength: {
        if (frame->selection()->isContentEditable()) {
            if (frame->document() && frame->document()->focusedNode()) {
                if (frame->document()->focusedNode()->hasTagName(HTMLNames::inputTag)) {
                    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(frame->document()->focusedNode());
                    return QVariant(inputElement->maxLength());
                }
            }
            return QVariant(HTMLInputElement::maximumLength);
        }
        return QVariant(0);
    }
    default:
        return QVariant();
    }
}

typedef struct {
    const char* name;
    double deferredRepaintDelay;
    double initialDeferredRepaintDelayDuringLoading;
    double maxDeferredRepaintDelayDuringLoading;
    double deferredRepaintDelayIncrementDuringLoading;
} QRepaintThrottlingPreset;

void QWebPageAdapter::dynamicPropertyChangeEvent(QObject* obj, QDynamicPropertyChangeEvent* event)
{
    if (event->propertyName() == "_q_viewMode") {
        page->setViewMode(Page::stringToViewMode(obj->property("_q_viewMode").toString()));
    } else if (event->propertyName() == "_q_HTMLTokenizerChunkSize") {
        int chunkSize = obj->property("_q_HTMLTokenizerChunkSize").toInt();
        page->setCustomHTMLTokenizerChunkSize(chunkSize);
    } else if (event->propertyName() == "_q_HTMLTokenizerTimeDelay") {
        double timeDelay = obj->property("_q_HTMLTokenizerTimeDelay").toDouble();
        page->setCustomHTMLTokenizerTimeDelay(timeDelay);
    } else if (event->propertyName() == "_q_RepaintThrottlingDeferredRepaintDelay") {
        double p = obj->property("_q_RepaintThrottlingDeferredRepaintDelay").toDouble();
        FrameView::setRepaintThrottlingDeferredRepaintDelay(p);
    } else if (event->propertyName() == "_q_RepaintThrottlingnInitialDeferredRepaintDelayDuringLoading") {
        double p = obj->property("_q_RepaintThrottlingnInitialDeferredRepaintDelayDuringLoading").toDouble();
        FrameView::setRepaintThrottlingnInitialDeferredRepaintDelayDuringLoading(p);
    } else if (event->propertyName() == "_q_RepaintThrottlingMaxDeferredRepaintDelayDuringLoading") {
        double p = obj->property("_q_RepaintThrottlingMaxDeferredRepaintDelayDuringLoading").toDouble();
        FrameView::setRepaintThrottlingMaxDeferredRepaintDelayDuringLoading(p);
    } else if (event->propertyName() == "_q_RepaintThrottlingDeferredRepaintDelayIncrementDuringLoading") {
        double p = obj->property("_q_RepaintThrottlingDeferredRepaintDelayIncrementDuringLoading").toDouble();
        FrameView::setRepaintThrottlingDeferredRepaintDelayIncrementDuringLoading(p);
    } else if (event->propertyName() == "_q_RepaintThrottlingPreset") {
        static const QRepaintThrottlingPreset presets[] = {
            {   "NoThrottling",     0,      0,      0,      0 },
            {   "Legacy",       0.025,      0,    2.5,    0.5 },
            {   "Minimal",       0.01,      0,      1,    0.2 },
            {   "Medium",       0.025,      1,      5,    0.5 },
            {   "Heavy",          0.1,      2,     10,      1 }
        };

        QString p = obj->property("_q_RepaintThrottlingPreset").toString();
        for (size_t i = 0; i < sizeof(presets) / sizeof(presets[0]); i++) {
            if (p == QLatin1String(presets[i].name)) {
                FrameView::setRepaintThrottlingDeferredRepaintDelay(presets[i].deferredRepaintDelay);
                FrameView::setRepaintThrottlingnInitialDeferredRepaintDelayDuringLoading(presets[i].initialDeferredRepaintDelayDuringLoading);
                FrameView::setRepaintThrottlingMaxDeferredRepaintDelayDuringLoading(presets[i].maxDeferredRepaintDelayDuringLoading);
                FrameView::setRepaintThrottlingDeferredRepaintDelayIncrementDuringLoading(presets[i].deferredRepaintDelayIncrementDuringLoading);
                break;
            }
        }
    } else if (event->propertyName() == "_q_webInspectorServerPort") {
#if ENABLE(INSPECTOR)
        QVariant port = obj->property("_q_webInspectorServerPort");
        if (port.isValid()) {
            InspectorServerQt* inspectorServer = InspectorServerQt::server();
            inspectorServer->listen(port.toInt());
        }
#endif
    } else if (event->propertyName() == "_q_deadDecodedDataDeletionInterval") {
        double interval = obj->property("_q_deadDecodedDataDeletionInterval").toDouble();
        memoryCache()->setDeadDecodedDataDeletionInterval(interval);
    }
}

#endif // QT_NO_DRAGANDDROP

#define MAP_ACTION_FROM_VALUE(Name, Value) \
    case Value: return QWebPageAdapter::Name

static QWebPageAdapter::MenuAction adapterActionForContextMenuAction(WebCore::ContextMenuAction action)
{
    switch (action) {
        FOR_EACH_MAPPED_MENU_ACTION(MAP_ACTION_FROM_VALUE, SEMICOLON_SEPARATOR);
#if ENABLE(INSPECTOR)
    case WebCore::ContextMenuItemTagInspectElement:
        return QWebPageAdapter::InspectElement;
#endif
    default:
        break;
    }
    return QWebPageAdapter::NoAction;
}

QList<MenuItem> descriptionForPlatformMenu(const QList<ContextMenuItem>* items, Page* page)
{
    QList<MenuItem> itemDescriptions;
    if (!items)
        return itemDescriptions;
    for (int i = 0; i < items->count(); ++i) {
        const ContextMenuItem &item = items->at(i);
        MenuItem description;
        switch (item.type()) {
        case WebCore::CheckableActionType: /* fall through */
        case WebCore::ActionType: {
            QWebPageAdapter::MenuAction action = adapterActionForContextMenuAction(item.action());
            if (action > QWebPageAdapter::NoAction) {
                description.type = MenuItem::Action;
                description.action = action;
                ContextMenuItem it(item);
                page->contextMenuController()->checkOrEnableIfNeeded(it);
                PlatformMenuItemDescription desc = it.releasePlatformDescription();
                if (desc.enabled)
                    description.traits |= MenuItem::Enabled;
                if (item.type() == WebCore::CheckableActionType) {
                    description.traits |= MenuItem::Checkable;
                    if (desc.checked)
                        description.traits |= MenuItem::Checked;
                }
            }
            break;
        }
        case WebCore::SeparatorType:
            description.type = MenuItem::Separator;
            break;
        case WebCore::SubmenuType: {
            description.type = MenuItem::SubMenu;
            description.subMenu = descriptionForPlatformMenu(item.platformSubMenu(), page);
            description.subMenuTitle = item.title();
            // Don't append empty submenu descriptions.
            if (description.subMenu.isEmpty())
                continue;
        }
        }
        if (description.type > MenuItem::NoType)
            itemDescriptions.append(description);
    }
    return itemDescriptions;
}

QWebHitTestResultPrivate* QWebPageAdapter::updatePositionDependentMenuActions(const QPoint& pos, QBitArray* visitedWebActions)
{
    ASSERT(visitedWebActions);
    WebCore::Frame* focusedFrame = page->focusController()->focusedOrMainFrame();
    HitTestResult result = focusedFrame->eventHandler()->hitTestResultAtPoint(focusedFrame->view()->windowToContents(pos));
    page->contextMenuController()->setHitTestResult(result);

#if ENABLE(INSPECTOR)
    if (page->inspectorController()->enabled())
        page->contextMenuController()->addInspectElementItem();
#endif

    WebCore::ContextMenu* webcoreMenu = page->contextMenuController()->contextMenu();
    QList<MenuItem> itemDescriptions;
    if (client && webcoreMenu)
        itemDescriptions = descriptionForPlatformMenu(webcoreMenu->platformDescription(), page);
    createAndSetCurrentContextMenu(itemDescriptions, visitedWebActions);
    if (result.scrollbar())
        return 0;
    return new QWebHitTestResultPrivate(result);
}

static void extractContentTypeFromHash(const HashSet<String>& types, QStringList* list)
{
    if (!list)
        return;

    HashSet<String>::const_iterator endIt = types.end();
    for (HashSet<String>::const_iterator it = types.begin(); it != endIt; ++it)
        *list << *it;
}

static void extractContentTypeFromPluginVector(const Vector<PluginPackage*>& plugins, QStringList* list)
{
    if (!list)
        return;

    for (unsigned i = 0; i < plugins.size(); ++i) {
        MIMEToDescriptionsMap::const_iterator it = plugins[i]->mimeToDescriptions().begin();
        MIMEToDescriptionsMap::const_iterator end = plugins[i]->mimeToDescriptions().end();
        for (; it != end; ++it)
            *list << it->key;
    }
}

QStringList QWebPageAdapter::supportedContentTypes() const
{
    QStringList mimeTypes;

    extractContentTypeFromHash(MIMETypeRegistry::getSupportedImageMIMETypes(), &mimeTypes);
    extractContentTypeFromHash(MIMETypeRegistry::getSupportedNonImageMIMETypes(), &mimeTypes);
    if (page->settings() && page->settings()->arePluginsEnabled())
        extractContentTypeFromPluginVector(PluginDatabase::installedPlugins()->plugins(), &mimeTypes);

    return mimeTypes;
}

void QWebPageAdapter::_q_cleanupLeakMessages()
{
#ifndef NDEBUG
    // Need this to make leak messages accurate.
    memoryCache()->setCapacities(0, 0, 0);
#endif
}

void QWebPageAdapter::_q_onLoadProgressChanged(int)
{
    m_totalBytes = page->progress()->totalPageAndResourceBytesToLoad();
    m_bytesReceived = page->progress()->totalBytesReceived();
}

bool QWebPageAdapter::supportsContentType(const QString& mimeType) const
{
    const String type = mimeType.toLower();
    if (MIMETypeRegistry::isSupportedImageMIMEType(type))
        return true;

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(type))
        return true;

    if (page->settings() && page->settings()->arePluginsEnabled()
        && PluginDatabase::installedPlugins()->isMIMETypeRegistered(type))
        return true;

    return false;
}

void QWebPageAdapter::didShowInspector()
{
#if ENABLE(INSPECTOR)
    page->inspectorController()->show();
#endif
}

void QWebPageAdapter::didCloseInspector()
{
#if ENABLE(INSPECTOR)
    page->inspectorController()->close();
#endif
}

void QWebPageAdapter::updateActionInternal(QWebPageAdapter::MenuAction action, const char* commandName, bool* enabled, bool* checked)
{
    WebCore::FrameLoader* loader = mainFrameAdapter()->frame->loader();
    WebCore::Editor* editor = page->focusController()->focusedOrMainFrame()->editor();

    switch (action) {
    case QWebPageAdapter::Back:
        *enabled = page->canGoBackOrForward(-1);
        break;
    case QWebPageAdapter::Forward:
        *enabled = page->canGoBackOrForward(1);
        break;
    case QWebPageAdapter::Stop:
        *enabled = loader->isLoading();
        break;
    case QWebPageAdapter::Reload:
        *enabled = !loader->isLoading();
        break;
    case QWebPageAdapter::SetTextDirectionDefault:
    case QWebPageAdapter::SetTextDirectionLeftToRight:
    case QWebPageAdapter::SetTextDirectionRightToLeft:
        *enabled = editor->canEdit();
        *checked = false;
        break;
    default: {

        // if it's an editor command, let its logic determine state
        if (commandName) {
            Editor::Command command = editor->command(commandName);
            *enabled = command.isEnabled();
            if (*enabled)
                *checked = command.state() != FalseTriState;
            else
                *checked = false;
        }
        break;
    }
    }
}

void QWebPageAdapter::triggerAction(QWebPageAdapter::MenuAction action, QWebHitTestResultPrivate* hitTestResult, const char* commandName, bool endToEndReload)
{
    Frame* frame = page->focusController()->focusedOrMainFrame();
    if (!frame)
        return;
    Editor* editor = frame->editor();

    // Convenience
    QWebHitTestResultPrivate hitTest;
    if (!hitTestResult)
        hitTestResult = &hitTest;

    switch (action) {
    case OpenLink:
        if (Frame* targetFrame = hitTestResult->webCoreFrame) {
            targetFrame->loader()->loadFrameRequest(frameLoadRequest(hitTestResult->linkUrl, targetFrame), /*lockHistory*/ false, /*lockBackForwardList*/ false, /*event*/ 0, /*FormState*/ 0, MaybeSendReferrer);
            break;
        }
        // fall through
    case OpenLinkInNewWindow:
        openNewWindow(hitTestResult->linkUrl, frame);
        break;
    case OpenLinkInThisWindow:
        frame->loader()->loadFrameRequest(frameLoadRequest(hitTestResult->linkUrl, frame), /*lockHistory*/ false, /*lockBackForwardList*/ false, /*event*/ 0, /*FormState*/ 0, MaybeSendReferrer);
        break;
    case OpenFrameInNewWindow: {
        KURL url = frame->loader()->documentLoader()->unreachableURL();
        if (url.isEmpty())
            url = frame->loader()->documentLoader()->url();
        openNewWindow(url, frame);
        break;
        }
    case CopyLinkToClipboard: {
#if defined(Q_WS_X11)
        bool oldSelectionMode = Pasteboard::generalPasteboard()->isSelectionMode();
        Pasteboard::generalPasteboard()->setSelectionMode(true);
        editor->copyURL(hitTestResult->linkUrl, hitTestResult->linkText);
        Pasteboard::generalPasteboard()->setSelectionMode(oldSelectionMode);
#endif
        editor->copyURL(hitTestResult->linkUrl, hitTestResult->linkText);
        break;
    }
    case OpenImageInNewWindow:
        openNewWindow(hitTestResult->imageUrl, frame);
        break;
    case DownloadImageToDisk:
        frame->loader()->client()->startDownload(WebCore::ResourceRequest(hitTestResult->imageUrl, frame->loader()->outgoingReferrer()));
        break;
    case DownloadLinkToDisk:
        frame->loader()->client()->startDownload(WebCore::ResourceRequest(hitTestResult->linkUrl, frame->loader()->outgoingReferrer()));
        break;
    case Back:
        page->goBack();
        break;
    case Forward:
        page->goForward();
        break;
    case Stop:
        mainFrameAdapter()->frame->loader()->stopForUserCancel();
        updateNavigationActions();
        break;
    case Reload:
        mainFrameAdapter()->frame->loader()->reload(endToEndReload);
        break;

    case SetTextDirectionDefault:
        editor->setBaseWritingDirection(NaturalWritingDirection);
        break;
    case SetTextDirectionLeftToRight:
        editor->setBaseWritingDirection(LeftToRightWritingDirection);
        break;
    case SetTextDirectionRightToLeft:
        editor->setBaseWritingDirection(RightToLeftWritingDirection);
        break;
#if ENABLE(INSPECTOR)
    case InspectElement: {
        ASSERT(hitTestResult != &hitTest);
        page->inspectorController()->inspect(hitTestResult->innerNonSharedNode);
        break;
    }
#endif
    default:
        if (commandName)
            editor->command(commandName).execute();
        break;
    }
}


QString QWebPageAdapter::contextMenuItemTagForAction(QWebPageAdapter::MenuAction action, bool* checkable) const
{
    ASSERT(checkable);
    switch (action) {
    case OpenLink:
        return contextMenuItemTagOpenLink();
    case OpenLinkInNewWindow:
        return contextMenuItemTagOpenLinkInNewWindow();
    case OpenFrameInNewWindow:
        return contextMenuItemTagOpenFrameInNewWindow();
    case OpenLinkInThisWindow:
        return contextMenuItemTagOpenLinkInThisWindow();

    case DownloadLinkToDisk:
        return contextMenuItemTagDownloadLinkToDisk();
    case CopyLinkToClipboard:
        return contextMenuItemTagCopyLinkToClipboard();

    case OpenImageInNewWindow:
        return contextMenuItemTagOpenImageInNewWindow();
    case DownloadImageToDisk:
        return contextMenuItemTagDownloadImageToDisk();
    case CopyImageToClipboard:
        return contextMenuItemTagCopyImageToClipboard();
    case CopyImageUrlToClipboard:
        return contextMenuItemTagCopyImageUrlToClipboard();

    case Cut:
        return contextMenuItemTagCut();
    case Copy:
        return contextMenuItemTagCopy();
    case Paste:
        return contextMenuItemTagPaste();
    case SelectAll:
        return contextMenuItemTagSelectAll();

    case Back:
        return contextMenuItemTagGoBack();
    case Forward:
        return contextMenuItemTagGoForward();
    case Reload:
        return contextMenuItemTagReload();
    case Stop:
        return contextMenuItemTagStop();

    case SetTextDirectionDefault:
        return contextMenuItemTagDefaultDirection();
    case SetTextDirectionLeftToRight:
        *checkable = true;
        return contextMenuItemTagLeftToRight();
    case SetTextDirectionRightToLeft:
        *checkable = true;
        return contextMenuItemTagRightToLeft();

    case ToggleBold:
        *checkable = true;
        return contextMenuItemTagBold();
    case ToggleItalic:
        *checkable = true;
        return contextMenuItemTagItalic();
    case ToggleUnderline:
        *checkable = true;
        return contextMenuItemTagUnderline();

#if ENABLE(INSPECTOR)
    case InspectElement:
        return contextMenuItemTagInspectElement();
#endif
    default:
        ASSERT_NOT_REACHED();
        return QString();
    }
}

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
void QWebPageAdapter::setNotificationsAllowedForFrame(QWebFrameAdapter* frame, bool allowed)
{
    NotificationPresenterClientQt::notificationPresenter()->setNotificationsAllowedForFrame(frame->frame, allowed);
}

void QWebPageAdapter::addNotificationPresenterClient()
{
    NotificationPresenterClientQt::notificationPresenter()->addClient();
}

#ifndef QT_NO_SYSTEMTRAYICON
bool QWebPageAdapter::hasSystemTrayIcon() const
{
    return NotificationPresenterClientQt::notificationPresenter()->hasSystemTrayIcon();
}

void QWebPageAdapter::setSystemTrayIcon(QObject *icon)
{
    NotificationPresenterClientQt::notificationPresenter()->setSystemTrayIcon(icon);
}
#endif // QT_NO_SYSTEMTRAYICON
#endif // ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)

#if ENABLE(GEOLOCATION)
void QWebPageAdapter::setGeolocationEnabledForFrame(QWebFrameAdapter* frame, bool on)
{
    GeolocationPermissionClientQt::geolocationPermissionClient()->setPermission(frame, on);
}
#endif


QString QWebPageAdapter::defaultUserAgentString()
{
    return UserAgentQt::standardUserAgent("", WEBKIT_MAJOR_VERSION, WEBKIT_MINOR_VERSION);
}

bool QWebPageAdapter::treatSchemeAsLocal(const QString& scheme)
{
    return WebCore::SchemeRegistry::shouldTreatURLSchemeAsLocal(scheme);
}

QObject* QWebPageAdapter::currentFrame() const
{
    Frame* frame = page->focusController()->focusedOrMainFrame();
    return frame->loader()->networkingContext()->originatingObject();
}

bool QWebPageAdapter::hasFocusedNode() const
{
    bool hasFocus = false;
    Frame* frame = page->focusController()->focusedFrame();
    if (frame) {
        Document* document = frame->document();
        hasFocus = document && document->focusedNode();
    }
    return hasFocus;
}

QWebPageAdapter::ViewportAttributes QWebPageAdapter::viewportAttributesForSize(const QSize &availableSize, const QSize &deviceSize) const
{
    static const int desktopWidth = 980;

    float devicePixelRatio = qt_defaultDpi() / WebCore::ViewportArguments::deprecatedTargetDPI;

    WebCore::ViewportAttributes conf = WebCore::computeViewportAttributes(viewportArguments(), desktopWidth, deviceSize.width(), deviceSize.height(), devicePixelRatio, availableSize);
    WebCore::restrictMinimumScaleFactorToViewportSize(conf, availableSize, devicePixelRatio);
    WebCore::restrictScaleFactorToInitialScaleIfNotUserScalable(conf);

    page->setDeviceScaleFactor(devicePixelRatio);
    QWebPageAdapter::ViewportAttributes result;

    result.size = QSizeF(conf.layoutSize.width(), conf.layoutSize.height());
    result.initialScaleFactor = conf.initialScale;
    result.minimumScaleFactor = conf.minimumScale;
    result.maximumScaleFactor = conf.maximumScale;
    result.devicePixelRatio = devicePixelRatio;
    result.isUserScalable = static_cast<bool>(conf.userScalable);

    return result;
}


bool QWebPageAdapter::handleKeyEvent(QKeyEvent *ev)
{
    Frame* frame = page->focusController()->focusedOrMainFrame();
    return frame->eventHandler()->keyEvent(ev);
}

bool QWebPageAdapter::handleScrolling(QKeyEvent *ev)
{
    Frame* frame = page->focusController()->focusedOrMainFrame();
    WebCore::ScrollDirection direction;
    WebCore::ScrollGranularity granularity;

#ifndef QT_NO_SHORTCUT
    if (ev == QKeySequence::MoveToNextPage || (ev->key() == Qt::Key_Space && !(ev->modifiers() & Qt::ShiftModifier))) {
        granularity = WebCore::ScrollByPage;
        direction = WebCore::ScrollDown;
    } else if (ev == QKeySequence::MoveToPreviousPage || ((ev->key() == Qt::Key_Space) && (ev->modifiers() & Qt::ShiftModifier))) {
        granularity = WebCore::ScrollByPage;
        direction = WebCore::ScrollUp;
    } else
#endif // QT_NO_SHORTCUT
    if ((ev->key() == Qt::Key_Up && ev->modifiers() & Qt::ControlModifier) || ev->key() == Qt::Key_Home) {
        granularity = WebCore::ScrollByDocument;
        direction = WebCore::ScrollUp;
    } else if ((ev->key() == Qt::Key_Down && ev->modifiers() & Qt::ControlModifier) || ev->key() == Qt::Key_End) {
        granularity = WebCore::ScrollByDocument;
        direction = WebCore::ScrollDown;
    } else {
        switch (ev->key()) {
        case Qt::Key_Up:
            granularity = WebCore::ScrollByLine;
            direction = WebCore::ScrollUp;
            break;
        case Qt::Key_Down:
            granularity = WebCore::ScrollByLine;
            direction = WebCore::ScrollDown;
            break;
        case Qt::Key_Left:
            granularity = WebCore::ScrollByLine;
            direction = WebCore::ScrollLeft;
            break;
        case Qt::Key_Right:
            granularity = WebCore::ScrollByLine;
            direction = WebCore::ScrollRight;
            break;
        default:
            return false;
        }
    }

    return frame->eventHandler()->scrollRecursively(direction, granularity);
}

void QWebPageAdapter::focusInEvent(QFocusEvent *)
{
    FocusController* focusController = page->focusController();
    focusController->setActive(true);
    focusController->setFocused(true);
    if (!focusController->focusedFrame())
        focusController->setFocusedFrame(mainFrameAdapter()->frame);
}

void QWebPageAdapter::focusOutEvent(QFocusEvent *)
{
    // only set the focused frame inactive so that we stop painting the caret
    // and the focus frame. But don't tell the focus controller so that upon
    // focusInEvent() we can re-activate the frame.
    FocusController* focusController = page->focusController();
    // Call setFocused first so that window.onblur doesn't get called twice
    focusController->setFocused(false);
    focusController->setActive(false);
}

bool QWebPageAdapter::handleShortcutOverrideEvent(QKeyEvent* event)
{
    WebCore::Frame* frame = page->focusController()->focusedOrMainFrame();
    WebCore::Editor* editor = frame->editor();
    if (!editor->canEdit())
        return false;
    if (event->modifiers() == Qt::NoModifier
        || event->modifiers() == Qt::ShiftModifier
        || event->modifiers() == Qt::KeypadModifier) {
        if (event->key() < Qt::Key_Escape)
            event->accept();
        else {
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
    return true;
}

bool QWebPageAdapter::touchEvent(QTouchEvent* event)
{
#if ENABLE(TOUCH_EVENTS)
    Frame* frame = mainFrameAdapter()->frame;
    if (!frame->view())
        return false;

    // Always accept the QTouchEvent so that we'll receive also TouchUpdate and TouchEnd events
    event->setAccepted(true);

    // Return whether the default action was cancelled in the JS event handler
    return frame->eventHandler()->handleTouchEvent(convertTouchEvent(event));
#else
    event->ignore();
    return false;
#endif
}

bool QWebPageAdapter::swallowContextMenuEvent(QContextMenuEvent *event, QWebFrameAdapter *webFrame)
{
    // Check the first and last enum values match at least, since we cast.
    ASSERT(int(QWebPageAdapter::ScrollUp) == int(WebCore::ScrollUp));
    ASSERT(int(QWebPageAdapter::ScrollRight) == int(WebCore::ScrollRight));
    ASSERT(int(QWebPageAdapter::ScrollByLine) == int(WebCore::ScrollByLine));
    ASSERT(int(QWebPageAdapter::ScrollByDocument) == int(WebCore::ScrollByDocument));

    page->contextMenuController()->clearContextMenu();

    if (webFrame) {
        Frame* frame = webFrame->frame;
        if (Scrollbar* scrollBar = frame->view()->scrollbarAtPoint(convertMouseEvent(event, 1).position())) {
            bool horizontal = (scrollBar->orientation() == HorizontalScrollbar);
            QWebPageAdapter::ScrollDirection direction = QWebPageAdapter::InvalidScrollDirection;
            QWebPageAdapter::ScrollGranularity granularity = QWebPageAdapter::InvalidScrollGranularity;
            bool scroll = handleScrollbarContextMenuEvent(event, horizontal, &direction, &granularity);
            if (!scroll)
                return true;
            if (direction == QWebPageAdapter::InvalidScrollDirection || granularity == QWebPageAdapter::InvalidScrollGranularity) {
                ScrollbarTheme* theme = scrollBar->theme();
                // Set the pressed position to the middle of the thumb so that when we
                // do move, the delta will be from the current pixel position of the
                // thumb to the new position
                int position = theme->trackPosition(scrollBar) + theme->thumbPosition(scrollBar) + theme->thumbLength(scrollBar) / 2;
                scrollBar->setPressedPos(position);
                const QPoint pos = scrollBar->convertFromContainingWindow(event->pos());
                scrollBar->moveThumb(horizontal ? pos.x() : pos.y());
            } else
                scrollBar->scrollableArea()->scroll(WebCore::ScrollDirection(direction), WebCore::ScrollGranularity(granularity));
            return true;
        }
    }

    WebCore::Frame* focusedFrame = page->focusController()->focusedOrMainFrame();
    focusedFrame->eventHandler()->sendContextMenuEvent(convertMouseEvent(event, 1));
    ContextMenu* menu = page->contextMenuController()->contextMenu();
    // If the website defines its own handler then sendContextMenuEvent takes care of
    // calling/showing it and the context menu pointer will be zero. This is the case
    // on maps.google.com for example.

    return !menu;
}
