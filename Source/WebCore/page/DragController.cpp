/*
 * Copyright (C) 2007, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "DragController.h"

#if ENABLE(DRAG_SUPPORT)
#include "CSSMutableStyleDeclaration.h"
#include "Clipboard.h"
#include "ClipboardAccessPolicy.h"
#include "CachedResourceLoader.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DragActions.h"
#include "DragClient.h"
#include "DragData.h"
#include "DragSession.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Element.h"
#include "EventHandler.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "HTMLAnchorElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "Image.h"
#include "MoveSelectionCommand.h"
#include "Node.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "RenderFileUploadControl.h"
#include "RenderImage.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include "ReplaceSelectionCommand.h"
#include "ResourceRequest.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "Text.h"
#include "TextEvent.h"
#include "htmlediting.h"
#include "markup.h"
#include <wtf/CurrentTime.h>
#include <wtf/RefPtr.h>

namespace WebCore {

static PlatformMouseEvent createMouseEvent(DragData* dragData)
{
    bool shiftKey, ctrlKey, altKey, metaKey;
    shiftKey = ctrlKey = altKey = metaKey = false;
    PlatformKeyboardEvent::getCurrentModifierState(shiftKey, ctrlKey, altKey, metaKey);
    return PlatformMouseEvent(dragData->clientPosition(), dragData->globalPosition(),
                              LeftButton, PlatformEvent::MouseMoved, 0, shiftKey, ctrlKey, altKey,
                              metaKey, currentTime());
}

DragController::DragController(Page* page, DragClient* client)
    : m_page(page)
    , m_client(client)
    , m_documentUnderMouse(0)
    , m_dragInitiator(0)
    , m_fileInputElementUnderMouse(0)
    , m_dragDestinationAction(DragDestinationActionNone)
    , m_dragSourceAction(DragSourceActionNone)
    , m_didInitiateDrag(false)
    , m_isHandlingDrag(false)
    , m_sourceDragOperation(DragOperationNone)
{
}

DragController::~DragController()
{
    m_client->dragControllerDestroyed();
}

PassOwnPtr<DragController> DragController::create(Page* page, DragClient* client)
{
    return adoptPtr(new DragController(page, client));
}

static PassRefPtr<DocumentFragment> documentFragmentFromDragData(DragData* dragData, Frame* frame, RefPtr<Range> context,
                                          bool allowPlainText, bool& chosePlainText)
{
    ASSERT(dragData);
    chosePlainText = false;

    Document* document = context->ownerDocument();
    ASSERT(document);
    if (document && dragData->containsCompatibleContent()) {
        if (PassRefPtr<DocumentFragment> fragment = dragData->asFragment(frame, context, allowPlainText, chosePlainText))
            return fragment;

        if (dragData->containsURL(frame, DragData::DoNotConvertFilenames)) {
            String title;
            String url = dragData->asURL(frame, DragData::DoNotConvertFilenames, &title);
            if (!url.isEmpty()) {
                RefPtr<HTMLAnchorElement> anchor = HTMLAnchorElement::create(document);
                anchor->setHref(url);
                if (title.isEmpty()) {
                    // Try the plain text first because the url might be normalized or escaped.
                    if (dragData->containsPlainText())
                        title = dragData->asPlainText(frame);
                    if (title.isEmpty())
                        title = url;
                }
                RefPtr<Node> anchorText = document->createTextNode(title);
                ExceptionCode ec;
                anchor->appendChild(anchorText, ec);
                RefPtr<DocumentFragment> fragment = document->createDocumentFragment();
                fragment->appendChild(anchor, ec);
                return fragment.get();
            }
        }
    }
    if (allowPlainText && dragData->containsPlainText()) {
        chosePlainText = true;
        return createFragmentFromText(context.get(), dragData->asPlainText(frame)).get();
    }

    return 0;
}

bool DragController::dragIsMove(FrameSelection* selection, DragData* dragData)
{
    return m_documentUnderMouse == m_dragInitiator && selection->isContentEditable() && selection->isRange() && !isCopyKeyDown(dragData);
}

// FIXME: This method is poorly named.  We're just clearing the selection from the document this drag is exiting.
void DragController::cancelDrag()
{
    m_page->dragCaretController()->clear();
}

void DragController::dragEnded()
{
    m_dragInitiator = 0;
    m_didInitiateDrag = false;
    m_page->dragCaretController()->clear();
    
    m_client->dragEnded();
}

DragSession DragController::dragEntered(DragData* dragData)
{
    return dragEnteredOrUpdated(dragData);
}

void DragController::dragExited(DragData* dragData)
{
    ASSERT(dragData);
    Frame* mainFrame = m_page->mainFrame();

    if (RefPtr<FrameView> v = mainFrame->view()) {
        ClipboardAccessPolicy policy = (!m_documentUnderMouse || m_documentUnderMouse->securityOrigin()->isLocal()) ? ClipboardReadable : ClipboardTypesReadable;
        RefPtr<Clipboard> clipboard = Clipboard::create(policy, dragData, mainFrame);
        clipboard->setSourceOperation(dragData->draggingSourceOperationMask());
        mainFrame->eventHandler()->cancelDragAndDrop(createMouseEvent(dragData), clipboard.get());
        clipboard->setAccessPolicy(ClipboardNumb);    // invalidate clipboard here for security
    }
    mouseMovedIntoDocument(0);
    if (m_fileInputElementUnderMouse)
        m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(false);
    m_fileInputElementUnderMouse = 0;
}

DragSession DragController::dragUpdated(DragData* dragData)
{
    return dragEnteredOrUpdated(dragData);
}

bool DragController::performDrag(DragData* dragData)
{
    ASSERT(dragData);
    m_documentUnderMouse = m_page->mainFrame()->documentAtPoint(dragData->clientPosition());
    if (m_dragDestinationAction & DragDestinationActionDHTML) {
        m_client->willPerformDragDestinationAction(DragDestinationActionDHTML, dragData);
        RefPtr<Frame> mainFrame = m_page->mainFrame();
        bool preventedDefault = false;
        if (mainFrame->view()) {
            // Sending an event can result in the destruction of the view and part.
            RefPtr<Clipboard> clipboard = Clipboard::create(ClipboardReadable, dragData, mainFrame.get());
            clipboard->setSourceOperation(dragData->draggingSourceOperationMask());
            preventedDefault = mainFrame->eventHandler()->performDragAndDrop(createMouseEvent(dragData), clipboard.get());
            clipboard->setAccessPolicy(ClipboardNumb); // Invalidate clipboard here for security
        }
        if (m_isHandlingDrag || preventedDefault) {
            m_documentUnderMouse = 0;
            return true;
        }
    }

    if ((m_dragDestinationAction & DragDestinationActionEdit) && concludeEditDrag(dragData)) {
        m_documentUnderMouse = 0;
        return true;
    }

    m_documentUnderMouse = 0;

    if (operationForLoad(dragData) == DragOperationNone)
        return false;

    m_client->willPerformDragDestinationAction(DragDestinationActionLoad, dragData);
    m_page->mainFrame()->loader()->load(ResourceRequest(dragData->asURL(m_page->mainFrame())), false);
    return true;
}

void DragController::mouseMovedIntoDocument(Document* newDocument)
{
    if (m_documentUnderMouse == newDocument)
        return;

    // If we were over another document clear the selection
    if (m_documentUnderMouse)
        cancelDrag();
    m_documentUnderMouse = newDocument;
}

DragSession DragController::dragEnteredOrUpdated(DragData* dragData)
{
    ASSERT(dragData);
    ASSERT(m_page->mainFrame()); // It is not possible in Mac WebKit to have a Page without a mainFrame()
    mouseMovedIntoDocument(m_page->mainFrame()->documentAtPoint(dragData->clientPosition()));

    m_dragDestinationAction = m_client->actionMaskForDrag(dragData);
    if (m_dragDestinationAction == DragDestinationActionNone) {
        cancelDrag(); // FIXME: Why not call mouseMovedIntoDocument(0)?
        return DragSession();
    }

    DragSession dragSession;
    bool handledByDocument = tryDocumentDrag(dragData, m_dragDestinationAction, dragSession);
    if (!handledByDocument && (m_dragDestinationAction & DragDestinationActionLoad))
        dragSession.operation = operationForLoad(dragData);
    return dragSession;
}

static HTMLInputElement* asFileInput(Node* node)
{
    ASSERT(node);

    HTMLInputElement* inputElement = node->toInputElement();

    // If this is a button inside of the a file input, move up to the file input.
    if (inputElement && inputElement->isTextButton() && inputElement->treeScope()->isShadowRoot())
        inputElement = inputElement->treeScope()->shadowHost()->toInputElement();

    return inputElement && inputElement->isFileUpload() ? inputElement : 0;
}

// This can return null if an empty document is loaded.
static Element* elementUnderMouse(Document* documentUnderMouse, const IntPoint& p)
{
    Frame* frame = documentUnderMouse->frame();
    float zoomFactor = frame ? frame->pageZoomFactor() : 1;
    LayoutPoint point = roundedLayoutPoint(FloatPoint(p.x() * zoomFactor, p.y() * zoomFactor));

    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active);
    HitTestResult result(point);
    documentUnderMouse->renderView()->layer()->hitTest(request, result);

    Node* n = result.innerNode();
    while (n && !n->isElementNode())
        n = n->parentNode();
    if (n)
        n = n->shadowAncestorNode();

    return static_cast<Element*>(n);
}

bool DragController::tryDocumentDrag(DragData* dragData, DragDestinationAction actionMask, DragSession& dragSession)
{
    ASSERT(dragData);

    if (!m_documentUnderMouse)
        return false;

    if (m_dragInitiator && !m_documentUnderMouse->securityOrigin()->canReceiveDragData(m_dragInitiator->securityOrigin()))
        return false;

    m_isHandlingDrag = false;
    if (actionMask & DragDestinationActionDHTML) {
        m_isHandlingDrag = tryDHTMLDrag(dragData, dragSession.operation);
        // Do not continue if m_documentUnderMouse has been reset by tryDHTMLDrag.
        // tryDHTMLDrag fires dragenter event. The event listener that listens
        // to this event may create a nested message loop (open a modal dialog),
        // which could process dragleave event and reset m_documentUnderMouse in
        // dragExited.
        if (!m_documentUnderMouse)
            return false;
    }

    // It's unclear why this check is after tryDHTMLDrag.
    // We send drag events in tryDHTMLDrag and that may be the reason.
    RefPtr<FrameView> frameView = m_documentUnderMouse->view();
    if (!frameView)
        return false;

    if (m_isHandlingDrag) {
        m_page->dragCaretController()->clear();
        return true;
    } else if ((actionMask & DragDestinationActionEdit) && canProcessDrag(dragData)) {
        if (dragData->containsColor()) {
            dragSession.operation = DragOperationGeneric;
            return true;
        }

        IntPoint point = frameView->windowToContents(dragData->clientPosition());
        Element* element = elementUnderMouse(m_documentUnderMouse.get(), point);
        if (!element)
            return false;
        
        HTMLInputElement* elementAsFileInput = asFileInput(element);
        if (m_fileInputElementUnderMouse != elementAsFileInput) {
            if (m_fileInputElementUnderMouse)
                m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(false);
            m_fileInputElementUnderMouse = elementAsFileInput;
        }
        
        if (!m_fileInputElementUnderMouse)
            m_page->dragCaretController()->setCaretPosition(m_documentUnderMouse->frame()->visiblePositionForPoint(point));

        Frame* innerFrame = element->document()->frame();
        dragSession.operation = dragIsMove(innerFrame->selection(), dragData) ? DragOperationMove : DragOperationCopy;
        dragSession.mouseIsOverFileInput = m_fileInputElementUnderMouse;
        dragSession.numberOfItemsToBeAccepted = 0;

        unsigned numberOfFiles = dragData->numberOfFiles();
        if (m_fileInputElementUnderMouse) {
            if (m_fileInputElementUnderMouse->disabled())
                dragSession.numberOfItemsToBeAccepted = 0;
            else if (m_fileInputElementUnderMouse->multiple())
                dragSession.numberOfItemsToBeAccepted = numberOfFiles;
            else
                dragSession.numberOfItemsToBeAccepted = 1;
            
            if (!dragSession.numberOfItemsToBeAccepted)
                dragSession.operation = DragOperationNone;
            m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(dragSession.numberOfItemsToBeAccepted);
        } else {
            // We are not over a file input element. The dragged item(s) will only
            // be loaded into the view the number of dragged items is 1.
            dragSession.numberOfItemsToBeAccepted = numberOfFiles != 1 ? 0 : 1;
        }
        
        return true;
    }
    
    // We are not over an editable region. Make sure we're clearing any prior drag cursor.
    m_page->dragCaretController()->clear();
    if (m_fileInputElementUnderMouse)
        m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(false);
    m_fileInputElementUnderMouse = 0;
    return false;
}

DragSourceAction DragController::delegateDragSourceAction(const IntPoint& rootViewPoint)
{
    m_dragSourceAction = m_client->dragSourceActionMaskForPoint(rootViewPoint);
    return m_dragSourceAction;
}

DragOperation DragController::operationForLoad(DragData* dragData)
{
    ASSERT(dragData);
    Document* doc = m_page->mainFrame()->documentAtPoint(dragData->clientPosition());
    if (doc && (m_didInitiateDrag || doc->isPluginDocument() || doc->rendererIsEditable()))
        return DragOperationNone;
    return dragOperation(dragData);
}

static bool setSelectionToDragCaret(Frame* frame, VisibleSelection& dragCaret, RefPtr<Range>& range, const IntPoint& point)
{
    frame->selection()->setSelection(dragCaret);
    if (frame->selection()->isNone()) {
        dragCaret = frame->visiblePositionForPoint(point);
        frame->selection()->setSelection(dragCaret);
        range = dragCaret.toNormalizedRange();
    }
    return !frame->selection()->isNone() && frame->selection()->isContentEditable();
}

bool DragController::dispatchTextInputEventFor(Frame* innerFrame, DragData* dragData)
{
    ASSERT(m_page->dragCaretController()->hasCaret());
    String text = m_page->dragCaretController()->isContentRichlyEditable() ? "" : dragData->asPlainText(innerFrame);
    Node* target = innerFrame->editor()->findEventTargetFrom(m_page->dragCaretController()->caretPosition());
    ExceptionCode ec = 0;
    return target->dispatchEvent(TextEvent::createForDrop(innerFrame->domWindow(), text), ec);
}

bool DragController::concludeEditDrag(DragData* dragData)
{
    ASSERT(dragData);
    ASSERT(!m_isHandlingDrag);

    if (m_fileInputElementUnderMouse) {
        m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(false);
        m_fileInputElementUnderMouse = 0;
    }

    if (!m_documentUnderMouse)
        return false;

    IntPoint point = m_documentUnderMouse->view()->windowToContents(dragData->clientPosition());
    Element* element = elementUnderMouse(m_documentUnderMouse.get(), point);
    if (!element)
        return false;
    Frame* innerFrame = element->ownerDocument()->frame();
    ASSERT(innerFrame);

    if (m_page->dragCaretController()->hasCaret() && !dispatchTextInputEventFor(innerFrame, dragData))
        return true;

    if (dragData->containsColor()) {
        Color color = dragData->asColor();
        if (!color.isValid())
            return false;
        RefPtr<Range> innerRange = innerFrame->selection()->toNormalizedRange();
        RefPtr<CSSMutableStyleDeclaration> style = CSSMutableStyleDeclaration::create();
        style->setProperty(CSSPropertyColor, color.serialized(), false);
        if (!innerFrame->editor()->shouldApplyStyle(style.get(), innerRange.get()))
            return false;
        m_client->willPerformDragDestinationAction(DragDestinationActionEdit, dragData);
        innerFrame->editor()->applyStyle(style.get(), EditActionSetColor);
        return true;
    }

    if (!m_page->dragController()->canProcessDrag(dragData)) {
        m_page->dragCaretController()->clear();
        return false;
    }

    if (HTMLInputElement* fileInput = asFileInput(element)) {
        if (fileInput->disabled())
            return false;

        if (!dragData->containsFiles())
            return false;

        Vector<String> filenames;
        dragData->asFilenames(filenames);
        if (filenames.isEmpty())
            return false;

        fileInput->receiveDroppedFiles(filenames);
        return true;
    }

    VisibleSelection dragCaret = m_page->dragCaretController()->caretPosition();
    m_page->dragCaretController()->clear();
    RefPtr<Range> range = dragCaret.toNormalizedRange();
    RefPtr<Element> rootEditableElement = innerFrame->selection()->rootEditableElement();

    // For range to be null a WebKit client must have done something bad while
    // manually controlling drag behaviour
    if (!range)
        return false;
    CachedResourceLoader* cachedResourceLoader = range->ownerDocument()->cachedResourceLoader();
    ResourceCacheValidationSuppressor validationSuppressor(cachedResourceLoader);
    if (dragIsMove(innerFrame->selection(), dragData) || dragCaret.isContentRichlyEditable()) {
        bool chosePlainText = false;
        RefPtr<DocumentFragment> fragment = documentFragmentFromDragData(dragData, innerFrame, range, true, chosePlainText);
        if (!fragment || !innerFrame->editor()->shouldInsertFragment(fragment, range, EditorInsertActionDropped)) {
            return false;
        }

        m_client->willPerformDragDestinationAction(DragDestinationActionEdit, dragData);
        if (dragIsMove(innerFrame->selection(), dragData)) {
            // NSTextView behavior is to always smart delete on moving a selection,
            // but only to smart insert if the selection granularity is word granularity.
            bool smartDelete = innerFrame->editor()->smartInsertDeleteEnabled();
            bool smartInsert = smartDelete && innerFrame->selection()->granularity() == WordGranularity && dragData->canSmartReplace();
            applyCommand(MoveSelectionCommand::create(fragment, dragCaret.base(), smartInsert, smartDelete));
        } else {
            if (setSelectionToDragCaret(innerFrame, dragCaret, range, point)) {
                ReplaceSelectionCommand::CommandOptions options = ReplaceSelectionCommand::SelectReplacement | ReplaceSelectionCommand::PreventNesting;
                if (dragData->canSmartReplace())
                    options |= ReplaceSelectionCommand::SmartReplace;
                if (chosePlainText)
                    options |= ReplaceSelectionCommand::MatchStyle;
                applyCommand(ReplaceSelectionCommand::create(m_documentUnderMouse.get(), fragment, options));
            }
        }
    } else {
        String text = dragData->asPlainText(innerFrame);
        if (text.isEmpty() || !innerFrame->editor()->shouldInsertText(text, range.get(), EditorInsertActionDropped)) {
            return false;
        }

        m_client->willPerformDragDestinationAction(DragDestinationActionEdit, dragData);
        if (setSelectionToDragCaret(innerFrame, dragCaret, range, point))
            applyCommand(ReplaceSelectionCommand::create(m_documentUnderMouse.get(), createFragmentFromText(range.get(), text),  ReplaceSelectionCommand::SelectReplacement | ReplaceSelectionCommand::MatchStyle | ReplaceSelectionCommand::PreventNesting));
    }

    if (rootEditableElement) {
        if (Frame* frame = rootEditableElement->document()->frame())
            frame->eventHandler()->updateDragStateAfterEditDragIfNeeded(rootEditableElement.get());
    }

    return true;
}

bool DragController::canProcessDrag(DragData* dragData)
{
    ASSERT(dragData);

    if (!dragData->containsCompatibleContent())
        return false;

    IntPoint point = m_page->mainFrame()->view()->windowToContents(dragData->clientPosition());
    HitTestResult result = HitTestResult(point);
    if (!m_page->mainFrame()->contentRenderer())
        return false;

    result = m_page->mainFrame()->eventHandler()->hitTestResultAtPoint(point, true);

    if (!result.innerNonSharedNode())
        return false;

    if (dragData->containsFiles() && asFileInput(result.innerNonSharedNode()))
        return true;

    if (!result.innerNonSharedNode()->rendererIsEditable())
        return false;

    if (m_didInitiateDrag && m_documentUnderMouse == m_dragInitiator && result.isSelected())
        return false;

    return true;
}

static DragOperation defaultOperationForDrag(DragOperation srcOpMask)
{
    // This is designed to match IE's operation fallback for the case where
    // the page calls preventDefault() in a drag event but doesn't set dropEffect.
    if (srcOpMask == DragOperationEvery)
        return DragOperationCopy;
    if (srcOpMask == DragOperationNone)
        return DragOperationNone;
    if (srcOpMask & DragOperationMove || srcOpMask & DragOperationGeneric)
        return DragOperationMove;
    if (srcOpMask & DragOperationCopy)
        return DragOperationCopy;
    if (srcOpMask & DragOperationLink)
        return DragOperationLink;
    
    // FIXME: Does IE really return "generic" even if no operations were allowed by the source?
    return DragOperationGeneric;
}

bool DragController::tryDHTMLDrag(DragData* dragData, DragOperation& operation)
{
    ASSERT(dragData);
    ASSERT(m_documentUnderMouse);
    RefPtr<Frame> mainFrame = m_page->mainFrame();
    RefPtr<FrameView> viewProtector = mainFrame->view();
    if (!viewProtector)
        return false;

    ClipboardAccessPolicy policy = m_documentUnderMouse->securityOrigin()->isLocal() ? ClipboardReadable : ClipboardTypesReadable;
    RefPtr<Clipboard> clipboard = Clipboard::create(policy, dragData, mainFrame.get());
    DragOperation srcOpMask = dragData->draggingSourceOperationMask();
    clipboard->setSourceOperation(srcOpMask);

    PlatformMouseEvent event = createMouseEvent(dragData);
    if (!mainFrame->eventHandler()->updateDragAndDrop(event, clipboard.get())) {
        clipboard->setAccessPolicy(ClipboardNumb);    // invalidate clipboard here for security
        return false;
    }

    operation = clipboard->destinationOperation();
    if (clipboard->dropEffectIsUninitialized())
        operation = defaultOperationForDrag(srcOpMask);
    else if (!(srcOpMask & operation)) {
        // The element picked an operation which is not supported by the source
        operation = DragOperationNone;
    }

    clipboard->setAccessPolicy(ClipboardNumb);    // invalidate clipboard here for security
    return true;
}

Node* DragController::draggableNode(const Frame* src, Node* startNode, const IntPoint& dragOrigin, DragState& state) const
{
    state.m_dragType = (src->selection()->contains(dragOrigin)) ? DragSourceActionSelection : DragSourceActionNone;

    for (const RenderObject* renderer = startNode->renderer(); renderer; renderer = renderer->parent()) {
        Node* node = renderer->node();
        if (!node)
            // Anonymous render blocks don't correspond to actual DOM nodes, so we skip over them
            // for the purposes of finding a draggable node.
            continue;
        if (!(state.m_dragType & DragSourceActionSelection) && node->isTextNode() && node->canStartSelection())
            // In this case we have a click in the unselected portion of text. If this text is
            // selectable, we want to start the selection process instead of looking for a parent
            // to try to drag.
            return 0;
        if (node->isElementNode()) {
            EUserDrag dragMode = renderer->style()->userDrag();
            if ((m_dragSourceAction & DragSourceActionDHTML) && dragMode == DRAG_ELEMENT) {
                state.m_dragType = static_cast<DragSourceAction>(state.m_dragType | DragSourceActionDHTML);
                return node;
            }
            if (dragMode == DRAG_AUTO) {
                if ((m_dragSourceAction & DragSourceActionImage)
                    && node->hasTagName(HTMLNames::imgTag)
                    && src->settings()
                    && src->settings()->loadsImagesAutomatically()) {
                    state.m_dragType = static_cast<DragSourceAction>(state.m_dragType | DragSourceActionImage);
                    return node;
                }
                if ((m_dragSourceAction & DragSourceActionLink)
                    && node->hasTagName(HTMLNames::aTag)
                    && static_cast<HTMLAnchorElement*>(node)->isLiveLink()) {
                    state.m_dragType = static_cast<DragSourceAction>(state.m_dragType | DragSourceActionLink);
                    return node;
                }
            }
        }
    }

    // We either have nothing to drag or we have a selection and we're not over a draggable element.
    return (state.m_dragType & DragSourceActionSelection) ? startNode : 0;
}

static CachedImage* getCachedImage(Element* element)
{
    ASSERT(element);
    RenderObject* renderer = element->renderer();
    if (!renderer || !renderer->isImage())
        return 0;
    RenderImage* image = toRenderImage(renderer);
    return image->cachedImage();
}

static Image* getImage(Element* element)
{
    ASSERT(element);
    CachedImage* cachedImage = getCachedImage(element);
    // Don't use cachedImage->imageForRenderer() here as that may return BitmapImages for cached SVG Images.
    // Users of getImage() want access to the SVGImage, in order to figure out the filename extensions,
    // which would be empty when asking the cached BitmapImages.
    return (cachedImage && !cachedImage->errorOccurred()) ?
        cachedImage->image() : 0;
}

static void prepareClipboardForImageDrag(Frame* source, Clipboard* clipboard, Element* node, const KURL& linkURL, const KURL& imageURL, const String& label)
{
    if (node->isContentRichlyEditable()) {
        RefPtr<Range> range = source->document()->createRange();
        ExceptionCode ec = 0;
        range->selectNode(node, ec);
        ASSERT(!ec);
        source->selection()->setSelection(VisibleSelection(range.get(), DOWNSTREAM));
    }
    clipboard->declareAndWriteDragImage(node, !linkURL.isEmpty() ? linkURL : imageURL, label, source);
}

static IntPoint dragLocForDHTMLDrag(const IntPoint& mouseDraggedPoint, const IntPoint& dragOrigin, const IntPoint& dragImageOffset, bool isLinkImage)
{
    // dragImageOffset is the cursor position relative to the lower-left corner of the image.
#if PLATFORM(MAC)
    // We add in the Y dimension because we are a flipped view, so adding moves the image down.
    const int yOffset = dragImageOffset.y();
#else
    const int yOffset = -dragImageOffset.y();
#endif

    if (isLinkImage)
        return IntPoint(mouseDraggedPoint.x() - dragImageOffset.x(), mouseDraggedPoint.y() + yOffset);

    return IntPoint(dragOrigin.x() - dragImageOffset.x(), dragOrigin.y() + yOffset);
}

static IntPoint dragLocForSelectionDrag(Frame* src)
{
    IntRect draggingRect = enclosingIntRect(src->selection()->bounds());
    int xpos = draggingRect.maxX();
    xpos = draggingRect.x() < xpos ? draggingRect.x() : xpos;
    int ypos = draggingRect.maxY();
#if PLATFORM(MAC)
    // Deal with flipped coordinates on Mac
    ypos = draggingRect.y() > ypos ? draggingRect.y() : ypos;
#else
    ypos = draggingRect.y() < ypos ? draggingRect.y() : ypos;
#endif
    return IntPoint(xpos, ypos);
}

bool DragController::startDrag(Frame* src, const DragState& state, DragOperation srcOp, const PlatformMouseEvent& dragEvent, const IntPoint& dragOrigin)
{
    ASSERT(src);

    if (!src->view() || !src->contentRenderer())
        return false;

    HitTestResult hitTestResult = src->eventHandler()->hitTestResultAtPoint(dragOrigin, true);
    if (!state.m_dragSrc->contains(hitTestResult.innerNode()))
        // The original node being dragged isn't under the drag origin anymore... maybe it was
        // hidden or moved out from under the cursor. Regardless, we don't want to start a drag on
        // something that's not actually under the drag origin.
        return false;
    KURL linkURL = hitTestResult.absoluteLinkURL();
    KURL imageURL = hitTestResult.absoluteImageURL();

    IntPoint mouseDraggedPoint = src->view()->windowToContents(dragEvent.position());

    m_draggingImageURL = KURL();
    m_sourceDragOperation = srcOp;

    DragImageRef dragImage = 0;
    IntPoint dragLoc(0, 0);
    IntPoint dragImageOffset(0, 0);

    Clipboard* clipboard = state.m_dragClipboard.get();
    if (state.m_dragType == DragSourceActionDHTML)
        dragImage = clipboard->createDragImage(dragImageOffset);
    if (state.m_dragType == DragSourceActionSelection || !imageURL.isEmpty() || !linkURL.isEmpty())
        // Selection, image, and link drags receive a default set of allowed drag operations that
        // follows from:
        // http://trac.webkit.org/browser/trunk/WebKit/mac/WebView/WebHTMLView.mm?rev=48526#L3430
        m_sourceDragOperation = static_cast<DragOperation>(m_sourceDragOperation | DragOperationGeneric | DragOperationCopy);

    // We allow DHTML/JS to set the drag image, even if its a link, image or text we're dragging.
    // This is in the spirit of the IE API, which allows overriding of pasteboard data and DragOp.
    if (dragImage) {
        dragLoc = dragLocForDHTMLDrag(mouseDraggedPoint, dragOrigin, dragImageOffset, !linkURL.isEmpty());
        m_dragOffset = dragImageOffset;
    }

    bool startedDrag = true; // optimism - we almost always manage to start the drag

    Node* node = state.m_dragSrc.get();

    Image* image = getImage(static_cast<Element*>(node));
    if (state.m_dragType == DragSourceActionSelection) {
        if (!clipboard->hasData()) {
            if (enclosingTextFormControl(src->selection()->start()))
                clipboard->writePlainText(src->editor()->selectedText());
            else {
                RefPtr<Range> selectionRange = src->selection()->toNormalizedRange();
                ASSERT(selectionRange);

                clipboard->writeRange(selectionRange.get(), src);
            }
        }
        m_client->willPerformDragSourceAction(DragSourceActionSelection, dragOrigin, clipboard);
        if (!dragImage) {
            dragImage = createDragImageForSelection(src);
            dragLoc = dragLocForSelectionDrag(src);
            m_dragOffset = IntPoint(dragOrigin.x() - dragLoc.x(), dragOrigin.y() - dragLoc.y());
        }
        doSystemDrag(dragImage, dragLoc, dragOrigin, clipboard, src, false);
    } else if (!imageURL.isEmpty() && node && node->isElementNode() && image && !image->isNull()
               && (m_dragSourceAction & DragSourceActionImage)) {
        // We shouldn't be starting a drag for an image that can't provide an extension.
        // This is an early detection for problems encountered later upon drop.
        ASSERT(!image->filenameExtension().isEmpty());
        Element* element = static_cast<Element*>(node);
        if (!clipboard->hasData()) {
            m_draggingImageURL = imageURL;
            prepareClipboardForImageDrag(src, clipboard, element, linkURL, imageURL, hitTestResult.altDisplayString());
        }

        m_client->willPerformDragSourceAction(DragSourceActionImage, dragOrigin, clipboard);

        if (!dragImage) {
            IntRect imageRect = hitTestResult.imageRect();
            imageRect.setLocation(m_page->mainFrame()->view()->rootViewToContents(src->view()->contentsToRootView(imageRect.location())));
            doImageDrag(element, dragOrigin, hitTestResult.imageRect(), clipboard, src, m_dragOffset);
        } else
            // DHTML defined drag image
            doSystemDrag(dragImage, dragLoc, dragOrigin, clipboard, src, false);

    } else if (!linkURL.isEmpty() && (m_dragSourceAction & DragSourceActionLink)) {
        if (!clipboard->hasData())
            // Simplify whitespace so the title put on the clipboard resembles what the user sees
            // on the web page. This includes replacing newlines with spaces.
            clipboard->writeURL(linkURL, hitTestResult.textContent().simplifyWhiteSpace(), src);

        if (src->selection()->isCaret() && src->selection()->isContentEditable()) {
            // a user can initiate a drag on a link without having any text
            // selected.  In this case, we should expand the selection to
            // the enclosing anchor element
            Position pos = src->selection()->base();
            Node* node = enclosingAnchorElement(pos);
            if (node)
                src->selection()->setSelection(VisibleSelection::selectionFromContentsOfNode(node));
        }

        m_client->willPerformDragSourceAction(DragSourceActionLink, dragOrigin, clipboard);
        if (!dragImage) {
            dragImage = createDragImageForLink(linkURL, hitTestResult.textContent(), src);
            IntSize size = dragImageSize(dragImage);
            m_dragOffset = IntPoint(-size.width() / 2, -LinkDragBorderInset);
            dragLoc = IntPoint(mouseDraggedPoint.x() + m_dragOffset.x(), mouseDraggedPoint.y() + m_dragOffset.y());
        }
        doSystemDrag(dragImage, dragLoc, mouseDraggedPoint, clipboard, src, true);
    } else if (state.m_dragType == DragSourceActionDHTML) {
        ASSERT(m_dragSourceAction & DragSourceActionDHTML);
        m_client->willPerformDragSourceAction(DragSourceActionDHTML, dragOrigin, clipboard);
        doSystemDrag(dragImage, dragLoc, dragOrigin, clipboard, src, false);
    } else {
        // draggableNode() determined an image or link node was draggable, but it turns out the
        // image or link had no URL, so there is nothing to drag.
        startedDrag = false;
    }

    if (dragImage)
        deleteDragImage(dragImage);
    return startedDrag;
}

void DragController::doImageDrag(Element* element, const IntPoint& dragOrigin, const IntRect& rect, Clipboard* clipboard, Frame* frame, IntPoint& dragImageOffset)
{
    IntPoint mouseDownPoint = dragOrigin;
    DragImageRef dragImage;
    IntPoint origin;

    Image* image = getImage(element);
    if (image && image->size().height() * image->size().width() <= MaxOriginalImageArea
        && (dragImage = createDragImageFromImage(image))) {
        IntSize originalSize = rect.size();
        origin = rect.location();

        dragImage = fitDragImageToMaxSize(dragImage, rect.size(), maxDragImageSize());
        dragImage = dissolveDragImageToFraction(dragImage, DragImageAlpha);
        IntSize newSize = dragImageSize(dragImage);

        // Properly orient the drag image and orient it differently if it's smaller than the original
        float scale = newSize.width() / (float)originalSize.width();
        float dx = origin.x() - mouseDownPoint.x();
        dx *= scale;
        origin.setX((int)(dx + 0.5));
#if PLATFORM(MAC)
        //Compensate for accursed flipped coordinates in cocoa
        origin.setY(origin.y() + originalSize.height());
#endif
        float dy = origin.y() - mouseDownPoint.y();
        dy *= scale;
        origin.setY((int)(dy + 0.5));
    } else {
        dragImage = createDragImageIconForCachedImage(getCachedImage(element));
        if (dragImage)
            origin = IntPoint(DragIconRightInset - dragImageSize(dragImage).width(), DragIconBottomInset);
    }

    dragImageOffset = mouseDownPoint + origin;
    doSystemDrag(dragImage, dragImageOffset, dragOrigin, clipboard, frame, false);

    deleteDragImage(dragImage);
}

void DragController::doSystemDrag(DragImageRef image, const IntPoint& dragLoc, const IntPoint& eventPos, Clipboard* clipboard, Frame* frame, bool forLink)
{
    m_didInitiateDrag = true;
    m_dragInitiator = frame->document();
    // Protect this frame and view, as a load may occur mid drag and attempt to unload this frame
    RefPtr<Frame> frameProtector = m_page->mainFrame();
    RefPtr<FrameView> viewProtector = frameProtector->view();
    m_client->startDrag(image, viewProtector->rootViewToContents(frame->view()->contentsToRootView(dragLoc)),
        viewProtector->rootViewToContents(frame->view()->contentsToRootView(eventPos)), clipboard, frameProtector.get(), forLink);

    cleanupAfterSystemDrag();
}

// Manual drag caret manipulation
void DragController::placeDragCaret(const IntPoint& windowPoint)
{
    mouseMovedIntoDocument(m_page->mainFrame()->documentAtPoint(windowPoint));
    if (!m_documentUnderMouse)
        return;
    Frame* frame = m_documentUnderMouse->frame();
    FrameView* frameView = frame->view();
    if (!frameView)
        return;
    IntPoint framePoint = frameView->windowToContents(windowPoint);

    m_page->dragCaretController()->setCaretPosition(frame->visiblePositionForPoint(framePoint));
}

} // namespace WebCore

#endif // ENABLE(DRAG_SUPPORT)
