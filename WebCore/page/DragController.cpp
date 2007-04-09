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
#include "DocLoader.h"
#include "DragActions.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Element.h"
#include "EventHandler.h"
#include "DragClient.h"
#include "DragData.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLAnchorElement.h"
#include "Image.h"
#include "markup.h"
#include "MoveSelectionCommand.h"
#include "Node.h"
#include "Page.h"
#include "PlugInInfoStore.h"
#include "RenderImage.h"
#include "ReplaceSelectionCommand.h"
#include "ResourceRequest.h"
#include "SelectionController.h"
#include "Settings.h"
#include "SystemTime.h"
#include "Text.h"
#include <wtf/RefPtr.h>

namespace WebCore {

static PlatformMouseEvent createMouseEvent(DragData* dragData)
{
    // FIXME: We should fake modifier keys here.
    return PlatformMouseEvent(dragData->clientPosition(), dragData->globalPosition(),
                              LeftButton, MouseEventMoved, 0, false, false, false, false, currentTime());

}
    
DragController::DragController(Page* page, DragClient* client)
    : m_page(page)
    , m_client(client)
    , m_document(0)
    , m_dragInitiator(0)
    , m_dragDestinationAction(DragDestinationActionNone)
    , m_dragSourceAction(DragSourceActionNone)
    , m_didInitiateDrag(false)
    , m_isHandlingDrag(false)
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

    Document* document = context->ownerDocument();
    ASSERT(document);
    if (document && dragData->containsCompatibleContent()) {
        if (PassRefPtr<DocumentFragment> fragment = dragData->asFragment(document))
            return fragment;

        if (dragData->containsURL()) {
            String title;
            String url = dragData->asURL(&title);
            if (!url.isEmpty()) {
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
    
    if (RefPtr<FrameView> v = mainFrame->view()) {
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
        RefPtr<Frame> mainFrame = m_page->mainFrame();
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
        if (operation == DragOperationNone && (m_dragDestinationAction & DragDestinationActionLoad))
            return operationForLoad(dragData);
    }
    
    return operation;
}
    
DragOperation DragController::tryDocumentDrag(DragData* dragData, DragDestinationAction actionMask)
{
    ASSERT(dragData);
    
    if (!m_document)
        return DragOperationNone;
    
    DragOperation operation = DragOperationNone;
    if (actionMask & DragDestinationActionDHTML)
        operation = tryDHTMLDrag(dragData);
    m_isHandlingDrag = operation != DragOperationNone; 

    RefPtr<FrameView> frameView = m_document->view();
    if (!frameView)
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

DragSourceAction DragController::delegateDragSourceAction(const IntPoint& windowPoint)
{  
    m_dragSourceAction = m_client->dragSourceActionMaskForPoint(windowPoint);
    return m_dragSourceAction;
}
    
DragOperation DragController::operationForLoad(DragData* dragData)
{
    ASSERT(dragData);
    Document* doc = documentAtPoint(m_page->mainFrame(), dragData->clientPosition());
    if (doc && (m_didInitiateDrag || doc->isPluginDocument() || (doc->frame() && doc->frame()->editor()->clientIsEditable())))
        return DragOperationNone;
    return dragOperation(dragData);
}

static bool setSelectionToDragCaret(Frame* frame, Selection& dragCaret, RefPtr<Range>& range, const IntPoint& point)
{
    frame->selectionController()->setSelection(dragCaret);
    if (frame->selectionController()->isNone()) {
        dragCaret = visiblePositionForPoint(frame, point);
        frame->selectionController()->setSelection(dragCaret);
        range = dragCaret.toRange();
    }
    return !frame->selectionController()->isNone() && frame->selectionController()->isContentEditable();
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
    DocLoader* loader = range->ownerDocument()->docLoader();
    loader->setAllowStaleResources(true);
    if (dragIsMove(innerFrame->selectionController(), dragData) || dragCaret.isContentRichlyEditable()) { 
        bool chosePlainText = false;
        RefPtr<DocumentFragment> fragment = documentFragmentFromDragData(dragData, range, true, chosePlainText);
        if (!fragment || !innerFrame->editor()->shouldInsertFragment(fragment, range, EditorInsertActionDropped)) {
            loader->setAllowStaleResources(false);
            return false;
        }
        
        m_client->willPerformDragDestinationAction(DragDestinationActionEdit, dragData);
        if (dragIsMove(innerFrame->selectionController(), dragData)) {
            bool smartMove = innerFrame->selectionGranularity() == WordGranularity 
                          && innerFrame->editor()->smartInsertDeleteEnabled() 
                          && dragData->canSmartReplace();
            applyCommand(new MoveSelectionCommand(fragment, dragCaret.base(), smartMove));
        } else {
            if (setSelectionToDragCaret(innerFrame, dragCaret, range, point))
                applyCommand(new ReplaceSelectionCommand(m_document, fragment, true, dragData->canSmartReplace(), chosePlainText)); 
        }    
    } else {
        String text = dragData->asPlainText();
        if (text.isEmpty() || !innerFrame->editor()->shouldInsertText(text, range.get(), EditorInsertActionDropped)) {
            loader->setAllowStaleResources(false);
            return false;
        }
        
        m_client->willPerformDragDestinationAction(DragDestinationActionEdit, dragData);
        if (setSelectionToDragCaret(innerFrame, dragCaret, range, point))
            applyCommand(new ReplaceSelectionCommand(m_document, createFragmentFromText(range.get(), text), true, false, true)); 
    }
    loader->setAllowStaleResources(false);

    return true;
}
    
    
bool DragController::canProcessDrag(DragData* dragData) 
{
    ASSERT(dragData);

    if (!dragData->containsCompatibleContent())
        return false;
    
    IntPoint point = m_page->mainFrame()->view()->windowToContents(dragData->clientPosition());
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
    ASSERT(m_document);
    DragOperation op = DragOperationNone;
    RefPtr<Frame> frame = m_page->mainFrame();
    RefPtr<FrameView> viewProtector = frame->view();
    if (!viewProtector)
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

bool DragController::mayStartDragAtEventLocation(const Frame* frame, const IntPoint& framePos)
{
    ASSERT(frame);

    if (!frame->view() || !frame->renderer())
        return false;

    HitTestResult mouseDownTarget = HitTestResult(framePos);

    mouseDownTarget = frame->eventHandler()->hitTestResultAtPoint(framePos, true);

    if (mouseDownTarget.image() 
        && !mouseDownTarget.absoluteImageURL().isEmpty()
        && frame->settings()->loadsImagesAutomatically()
        && m_dragSourceAction & DragSourceActionImage)
        return true;

    if (!mouseDownTarget.absoluteLinkURL().isEmpty()
        && m_dragSourceAction & DragSourceActionLink
        && mouseDownTarget.isLiveLink())
        return true;

    if (mouseDownTarget.isSelected()
        && m_dragSourceAction & DragSourceActionSelection)
        return true;

    return false;

}
    
static CachedImage* getCachedImage(Element* element)
{
    ASSERT(element);
    RenderObject* renderer = element->renderer();
    if (!renderer || !renderer->isImage()) 
        return 0;
    RenderImage* image = static_cast<RenderImage*>(renderer);
    return image->cachedImage();
}
    
static Image* getImage(Element* element)
{
    ASSERT(element);
    RenderObject* renderer = element->renderer();
    if (!renderer || !renderer->isImage()) 
        return 0;
    
    RenderImage* image = static_cast<RenderImage*>(renderer);
    if (image->cachedImage() && !image->cachedImage()->errorOccurred())
        return image->cachedImage()->image();
    return 0;
}
    
static void prepareClipboardForImageDrag(Frame* src, Clipboard* clipboard, Element* node, const KURL& linkURL, const KURL& imageURL, const String& label)
{
    RefPtr<Range> range = src->document()->createRange();
    ExceptionCode ec = 0;
    range->selectNode(node, ec);
    ASSERT(ec == 0);
    src->selectionController()->setSelection(Selection(range.get(), DOWNSTREAM));           
    clipboard->declareAndWriteDragImage(node, !linkURL.isEmpty() ? linkURL : imageURL, label, src);
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
    IntRect draggingRect = enclosingIntRect(src->visibleSelectionRect());
    int xpos = draggingRect.right();
    xpos = draggingRect.x() < xpos ? draggingRect.x() : xpos;
    int ypos = draggingRect.bottom();
#if PLATFORM(MAC)
    // Deal with flipped coordinates on Mac
    ypos = draggingRect.y() > ypos ? draggingRect.y() : ypos;
#else
    ypos = draggingRect.y() < ypos ? draggingRect.y() : ypos;
#endif
    return IntPoint(xpos, ypos);
}
    
bool DragController::startDrag(Frame* src, Clipboard* clipboard, DragOperation srcOp, const PlatformMouseEvent& dragEvent, const IntPoint& dragOrigin, bool isDHTMLDrag)
{    
    ASSERT(src);
    ASSERT(clipboard);
    
    if (!src->view() || !src->renderer())
        return false;
    
    HitTestResult dragSource = HitTestResult(dragOrigin);
    dragSource = src->eventHandler()->hitTestResultAtPoint(dragOrigin, true);
    KURL linkURL = dragSource.absoluteLinkURL();
    KURL imageURL = dragSource.absoluteImageURL();
    bool isSelected = dragSource.isSelected();
    
    IntPoint mouseDraggedPoint = src->view()->windowToContents(dragEvent.pos());
    
    m_draggingImageURL = KURL();
    m_dragOperation = srcOp;
    
    DragImageRef dragImage = 0;
    IntPoint dragLoc(0, 0);
    IntPoint dragImageOffset(0, 0);
    
    if (isDHTMLDrag) 
        dragImage = clipboard->createDragImage(dragImageOffset);
    
    // We allow DHTML/JS to set the drag image, even if its a link, image or text we're dragging.
    // This is in the spirit of the IE API, which allows overriding of pasteboard data and DragOp.
    if (dragImage) {
        dragLoc = dragLocForDHTMLDrag(mouseDraggedPoint, dragOrigin, dragImageOffset, !linkURL.isEmpty());
        m_dragOffset = dragImageOffset;
    }
    
    bool startedDrag = true; // optimism - we almost always manage to start the drag
    
    Node* node = dragSource.innerNonSharedNode();
    
    if (!imageURL.isEmpty() && node && node->isElementNode()
            && getImage(static_cast<Element*>(node)) 
            && (m_dragSourceAction & DragSourceActionImage)) {
        Element* element = static_cast<Element*>(node);
        if (!clipboard->hasData()) {
            m_draggingImageURL = imageURL; 
            prepareClipboardForImageDrag(src, clipboard, element, linkURL, imageURL, dragSource.altDisplayString());
        }
        
        m_client->willPerformDragSourceAction(DragSourceActionImage, dragOrigin, clipboard);
        
        if (!dragImage) {
            IntRect imageRect = dragSource.imageRect();
            imageRect.setLocation(m_page->mainFrame()->view()->windowToContents(src->view()->contentsToWindow(imageRect.location())));
            doImageDrag(element, dragOrigin, dragSource.imageRect(), clipboard, src, m_dragOffset);
        } else 
            // DHTML defined drag image
            doSystemDrag(dragImage, dragLoc, dragOrigin, clipboard, src, false);

    } else if (!linkURL.isEmpty() && (m_dragSourceAction & DragSourceActionLink)) {
        if (!clipboard->hasData())
            // Simplify whitespace so the title put on the clipboard resembles what the user sees
            // on the web page. This includes replacing newlines with spaces.
            clipboard->writeURL(linkURL, dragSource.textContent().simplifyWhiteSpace(), src);
        
        m_client->willPerformDragSourceAction(DragSourceActionLink, dragOrigin, clipboard);
        if (!dragImage) {
            dragImage = m_client->createDragImageForLink(linkURL, dragSource.textContent(), src);
            IntSize size = dragImageSize(dragImage);
            m_dragOffset = IntPoint(-size.width() / 2, -LinkDragBorderInset);
            dragLoc = IntPoint(mouseDraggedPoint.x() + m_dragOffset.x(), mouseDraggedPoint.y() + m_dragOffset.y());
        } 
        doSystemDrag(dragImage, dragLoc, mouseDraggedPoint, clipboard, src, true);
    } else if (isSelected && (m_dragSourceAction & DragSourceActionSelection)) {
        RefPtr<Range> selectionRange = src->selectionController()->toRange();
        ASSERT(selectionRange);
        if (!clipboard->hasData()) 
            clipboard->writeRange(selectionRange.get(), src);
        m_client->willPerformDragSourceAction(DragSourceActionSelection, dragOrigin, clipboard);
        if (!dragImage) {
            dragImage = createDragImageForSelection(src);
            dragLoc = dragLocForSelectionDrag(src);
            m_dragOffset = IntPoint((int)(dragOrigin.x() - dragLoc.x()), (int)(dragOrigin.y() - dragLoc.y()));
        }
        doSystemDrag(dragImage, dragLoc, dragOrigin, clipboard, src, false);
    } else if (isDHTMLDrag) {
        ASSERT(m_dragSourceAction & DragSourceActionDHTML);
        m_client->willPerformDragSourceAction(DragSourceActionDHTML, dragOrigin, clipboard);
        doSystemDrag(dragImage, dragLoc, dragOrigin, clipboard, src, false);
    } else {
        // Only way I know to get here is if to get here is if the original element clicked on in the mousedown is no longer
        // under the mousedown point, so linkURL, imageURL and isSelected are all false/empty.
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
        
        dragImage = fitDragImageToMaxSize(dragImage, maxDragImageSize());
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
    
    dragImageOffset.setX(mouseDownPoint.x() + origin.x());
    dragImageOffset.setY(mouseDownPoint.y() + origin.y());
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
    m_client->startDrag(image, viewProtector->windowToContents(frame->view()->contentsToWindow(dragLoc)),
        viewProtector->windowToContents(frame->view()->contentsToWindow(eventPos)), clipboard, frameProtector.get(), forLink);
    
    // Drag has ended, dragEnded *should* have been called, however it is possible  
    // for the UIDelegate to take over the drag, and fail to send the appropriate
    // drag termination event.  As dragEnded just resets drag variables, we just 
    // call it anyway to be on the safe side
    dragEnded();
}
    
}
