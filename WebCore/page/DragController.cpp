/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "Clipboard.h"
#include "ClipboardAccessPolicy.h"
#include "CSSStyleDeclaration.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DragActions.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Element.h"
#include "EventHandler.h"
#include "DragClient.h"
#include "DragData.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLAnchorElement.h"
#include "markup.h"
#include "MoveSelectionCommand.h"
#include "Node.h"
#include "Page.h"
#include "ReplaceSelectionCommand.h"
#include "ResourceRequest.h"
#include "SelectionController.h"
#include "Text.h"
#include "wtf/RefPtr.h"

namespace WebCore {

static PlatformMouseEvent createMouseEvent(DragData* dragData)
{
    // FIXME: We should fake modifier keys here.
    return PlatformMouseEvent(dragData->clientPosition(), dragData->globalPosition(),
                              LeftButton, 0, false, false, false, false);

}
    
DragController::DragController(Page* page, DragClient* client)
    : m_page(page)
    , m_client(client)
    , m_document(0)
    , m_dragInitiator(0)
    , m_dragDestinationAction(DragDestinationActionNone)
    , m_dragSourceAction(DragSourceActionNone)
    , m_didInitiateDrag(false)
    , m_dragOperation(DragOperationNone)
{
}
    
DragController::~DragController()
{   
    m_client->dragControllerDestroyed();
}
    
static PassRefPtr<DocumentFragment> documentFragmentFromDragData(DragData* dragData, RefPtr<Range> context,
                                          bool allowPlainText, bool chosePlainText)
{
    ASSERT(dragData);
    chosePlainText = false;

    Document* document = context->startNode()->document();
    ASSERT(document);
    if (document && dragData->containsCompatibleContent()) {
        if (DocumentFragment* fragment = dragData->asFragment(document).get())
            return fragment;

        if (dragData->containsURL()) {
            String title;
            String url = dragData->asURL(&title);
            if (document && !url.isEmpty()) {
                ExceptionCode ec;
                RefPtr<HTMLAnchorElement> anchor = static_cast<HTMLAnchorElement*>(document->createElement("a", ec).get());
                anchor->setHref(url);
                RefPtr<Node> anchorText = document->createTextNode(title);
                anchor->appendChild(anchorText, ec);
                RefPtr<DocumentFragment> fragment = document->createDocumentFragment();
                fragment->appendChild(anchor, ec);
                return fragment.get();
            }
        }
    }
    if (allowPlainText && dragData->containsPlainText()) {
        chosePlainText = true;
        return createFragmentFromText(context.get(), dragData->asPlainText()).get();
    }
    
    return 0;
}

bool DragController::dragIsMove(SelectionController* selectionController, DragData* dragData) 
{
    return m_document == m_dragInitiator
        && selectionController->isContentEditable()
        && !isCopyKeyDown();
}

static VisiblePosition visiblePositionForPoint(Frame* frame, IntPoint outerPoint)
{
    ASSERT(frame);
    HitTestResult result = frame->eventHandler()->hitTestResultAtPoint(outerPoint, true);
    Node* node = result.innerNode();
    if (!node)
        return VisiblePosition();
    RenderObject* renderer = node->renderer();
    if (!renderer)
        return VisiblePosition();
    VisiblePosition visiblePos = renderer->positionForCoordinates(result.localPoint().x(), result.localPoint().y());
    if (visiblePos.isNull())
        visiblePos = VisiblePosition(Position(node, 0));
    return visiblePos;
}

void DragController::cancelDrag()
{
    m_page->dragCaretController()->clear();
}

static Document* documentAtPoint(Frame* frame, const IntPoint& point)
{  
    if (!frame || !frame->view()) 
        return 0;

    IntPoint pt = frame->view()->windowToContents(point);
    HitTestResult result = HitTestResult(pt);
    
    if (frame->renderer())
        result = frame->eventHandler()->hitTestResultAtPoint(pt, false);
    return result.innerNode() ? result.innerNode()->document() : 0;
}
    
DragOperation DragController::dragEntered(DragData* dragData) 
{
    return dragEnteredOrUpdated(dragData);
}
    
void DragController::dragExited(DragData* dragData) 
{   
    ASSERT(dragData);
    Frame* mainFrame = m_page->mainFrame();
    
    if (mainFrame->view()) {
        ClipboardAccessPolicy policy = mainFrame->loader()->baseURL().isLocalFile() ? ClipboardReadable : ClipboardTypesReadable;
        RefPtr<Clipboard> clipboard = dragData->createClipboard(policy);
        clipboard->setSourceOperation(dragData->draggingSourceOperationMask());
        mainFrame->eventHandler()->cancelDragAndDrop(createMouseEvent(dragData), clipboard.get());
        clipboard->setAccessPolicy(ClipboardNumb);    // invalidate clipboard here for security
    }

    cancelDrag();
    m_document = 0;
}
    
DragOperation DragController::dragUpdated(DragData* dragData) 
{
    return dragEnteredOrUpdated(dragData);
}
    
bool DragController::performDrag(DragData* dragData)
{   
    ASSERT(dragData);
    ASSERT(m_document == documentAtPoint(m_page->mainFrame(), dragData->clientPosition()));
    if (m_isHandlingDrag) {
        ASSERT(m_dragDestinationAction & DragDestinationActionDHTML);
        m_client->willPerformDragDestinationAction(DragDestinationActionDHTML, dragData);
        Frame* mainFrame = m_page->mainFrame();
        if (mainFrame->view()) {
            // Sending an event can result in the destruction of the view and part.
            RefPtr<Clipboard> clipboard = dragData->createClipboard(ClipboardReadable);
            clipboard->setSourceOperation(dragData->draggingSourceOperationMask());
            mainFrame->eventHandler()->performDragAndDrop(createMouseEvent(dragData), clipboard.get());
            clipboard->setAccessPolicy(ClipboardNumb);    // invalidate clipboard here for security
        }
        m_document = 0;
        return true;
    } 
    
    if ((m_dragDestinationAction & DragDestinationActionEdit) && concludeDrag(dragData, m_dragDestinationAction)) {
        m_document = 0;
        return true;
    }
    
    m_document = 0;

    if (operationForLoad(dragData) == DragOperationNone)
        return false;
      
    m_page->mainFrame()->loader()->load(ResourceRequest(dragData->asURL()));
    return true;
}
    
DragOperation DragController::dragEnteredOrUpdated(DragData* dragData)
{
    ASSERT(dragData);
    IntPoint windowPoint = dragData->clientPosition();
    Document* newDraggingDoc = documentAtPoint(m_page->mainFrame(), windowPoint);
    if (m_document != newDraggingDoc) {
        if (m_document)
            cancelDrag();
        m_document = newDraggingDoc;
    }
    
    m_dragDestinationAction = m_client->actionMaskForDrag(dragData);
    
    DragOperation operation = DragOperationNone;
    
    if (m_dragDestinationAction == DragDestinationActionNone)
        cancelDrag();
    else {
        operation = tryDocumentDrag(dragData, m_dragDestinationAction);
        if (operation == DragOperationNone)
            return operationForLoad(dragData);
    }
    
    return operation;
}
    
DragOperation DragController::tryDocumentDrag(DragData* dragData, DragDestinationAction actionMask)
{
    ASSERT(dragData);
    DragOperation operation = DragOperationNone;
    if (actionMask & DragDestinationActionDHTML)
        operation = tryDHTMLDrag(dragData);
    m_isHandlingDrag = operation != DragOperationNone; 

    FrameView *frameView = 0;
    if (!m_document || !(frameView = m_document->view()))
        return operation;
    
    
    
    if ((actionMask & DragDestinationActionEdit) && !m_isHandlingDrag && canProcessDrag(dragData)) {
        if (dragData->containsColor()) 
            return DragOperationGeneric;
        
        IntPoint dragPos = dragData->clientPosition();
        IntPoint point = frameView->windowToContents(dragPos);
        Selection dragCaret(visiblePositionForPoint(m_document->frame(), point));
        m_page->dragCaretController()->setSelection(dragCaret);
        Element* element = m_document->elementFromPoint(point.x(), point.y());
        ASSERT(element);
        Frame* innerFrame = element->document()->frame();
        ASSERT(innerFrame);
        return dragIsMove(innerFrame->selectionController(), dragData) ? DragOperationMove : DragOperationCopy;
    } 
    
    m_page->dragCaretController()->clear();
    return operation;
}
    
DragOperation DragController::operationForLoad(DragData* dragData)
{
    ASSERT(dragData);
    Document* doc = documentAtPoint(m_page->mainFrame(), dragData->clientPosition());
    if (doc && (m_didInitiateDrag || doc->isPluginDocument() || (doc->frame() && doc->frame()->editor()->clientIsEditable())))
        return DragOperationNone;
    return dragOperation(dragData);
}

bool DragController::concludeDrag(DragData* dragData, DragDestinationAction actionMask)
{
    ASSERT(dragData);
    ASSERT(!m_isHandlingDrag);
    ASSERT(actionMask & DragDestinationActionEdit);
    
    if (!m_document)
        return false;
    
    IntPoint point = m_document->view()->windowToContents(dragData->clientPosition());
    Element* element =  m_document->elementFromPoint(point.x(), point.y());
    ASSERT(element);
    Frame* innerFrame = element->ownerDocument()->frame();
    ASSERT(innerFrame);    

    if (dragData->containsColor()) {
        Color color = dragData->asColor();
        if (!color.isValid())
            return false;
        if (!innerFrame)
            return false;
        RefPtr<Range> innerRange = innerFrame->selectionController()->toRange();
        RefPtr<CSSStyleDeclaration> style = m_document->createCSSStyleDeclaration();
        ExceptionCode ec;
        style->setProperty("color", color.name(), ec);
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

    Selection dragCaret(m_page->dragCaretController()->selection());
    m_page->dragCaretController()->clear();
    RefPtr<Range> range = dragCaret.toRange();
    if (dragIsMove(innerFrame->selectionController(), dragData) || dragCaret.isContentRichlyEditable()) { 
        bool chosePlainText = false;
        RefPtr<DocumentFragment> fragment = documentFragmentFromDragData(dragData, range, true, chosePlainText);
        if (!fragment || !innerFrame->editor()->shouldInsertFragment(fragment, range, EditorInsertActionDropped))
            return false;
        
        m_client->willPerformDragDestinationAction(DragDestinationActionEdit, dragData);
        if (dragIsMove(innerFrame->selectionController(), dragData)) {
            bool smartMove = innerFrame->selectionGranularity() == WordGranularity 
                          && innerFrame->editor()->smartInsertDeleteEnabled() 
                          && dragData->canSmartReplace();
            applyCommand(new MoveSelectionCommand(fragment, dragCaret.base(), smartMove));
        } else {
            innerFrame->selectionController()->setSelection(dragCaret);
            applyCommand(new ReplaceSelectionCommand(m_document, fragment, true, dragData->canSmartReplace(), chosePlainText)); 
        }    
    } else {
        String text = dragData->asPlainText();
        if (text.isEmpty() || !innerFrame->editor()->shouldInsertText(text, range.get(), EditorInsertActionDropped))
            return false;
        
        m_client->willPerformDragDestinationAction(DragDestinationActionEdit, dragData);
        innerFrame->selectionController()->setSelection(dragCaret);
        applyCommand(new ReplaceSelectionCommand(m_document, createFragmentFromText(range.get(), text), true, false, true)); 
    }

    return true;
}
    
    
bool DragController::canProcessDrag(DragData* dragData) 
{
    ASSERT(dragData);

    if (!dragData->containsCompatibleContent())
        return false;
    
    IntPoint point = m_document->view()->windowToContents(dragData->clientPosition());
    HitTestResult result = HitTestResult(point);
    
    if (!m_page->mainFrame()->renderer())
        return false;

    result = m_page->mainFrame()->eventHandler()->hitTestResultAtPoint(point, true);
    if (!result.innerNonSharedNode() || !result.innerNonSharedNode()->isContentEditable()) 
        return false;
    if (m_didInitiateDrag && m_document == m_dragInitiator && result.isSelected())
        return false;
    return true;
}

DragOperation DragController::tryDHTMLDrag(DragData* dragData)
{   
    ASSERT(dragData);
    DragOperation op = DragOperationNone;
    Frame* frame = m_page->mainFrame();
    if (!frame->view())
        return DragOperationNone;
    
    ClipboardAccessPolicy policy = frame->loader()->baseURL().isLocalFile() ? ClipboardReadable : ClipboardTypesReadable;
    RefPtr<Clipboard> clipboard = dragData->createClipboard(policy);
    DragOperation srcOp = dragData->draggingSourceOperationMask();
    clipboard->setSourceOperation(srcOp);
    
    PlatformMouseEvent event = createMouseEvent(dragData);
    if (frame->eventHandler()->updateDragAndDrop(event, clipboard.get())) {
        // *op unchanged if no source op was set
        if (!clipboard->destinationOperation(op)) {
            // The element accepted but they didn't pick an operation, so we pick one for them
            // (as does WinIE).
            if (srcOp & DragOperationCopy)
                op = DragOperationCopy;
            else if (srcOp & DragOperationMove || srcOp & DragOperationGeneric)
                op = DragOperationMove;
            else if (srcOp & DragOperationLink)
                op = DragOperationLink;
            else
                op = DragOperationGeneric;
        } else if (!(op & srcOp)) {
            op = DragOperationNone;
        }

        clipboard->setAccessPolicy(ClipboardNumb);    // invalidate clipboard here for security
        return op;
    }
    return op;
}

}
