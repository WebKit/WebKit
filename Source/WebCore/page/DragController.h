/*
 * Copyright (C) 2007-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "DragActions.h"
#include "DragImage.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "SimpleRange.h"
#include <wtf/URL.h>

namespace WebCore {

class DataTransfer;
class Document;
class DragClient;
class DragData;
class Element;
class Frame;
class FrameSelection;
class HTMLImageElement;
class HTMLInputElement;
class HitTestResult;
class IntRect;
class Page;
class PlatformMouseEvent;

struct DragItem;
struct DragState;
struct PromisedAttachmentInfo;

class DragController {
    WTF_MAKE_NONCOPYABLE(DragController); WTF_MAKE_FAST_ALLOCATED;
public:
    DragController(Page&, std::unique_ptr<DragClient>&&);
    ~DragController();

    static DragOperation platformGenericDragOperation();

    WEBCORE_EXPORT Optional<DragOperation> dragEntered(const DragData&);
    WEBCORE_EXPORT void dragExited(const DragData&);
    WEBCORE_EXPORT Optional<DragOperation> dragUpdated(const DragData&);
    WEBCORE_EXPORT bool performDragOperation(const DragData&);
    WEBCORE_EXPORT void dragCancelled();

    bool mouseIsOverFileInput() const { return m_fileInputElementUnderMouse; }
    unsigned numberOfItemsToBeAccepted() const { return m_numberOfItemsToBeAccepted; }

    // FIXME: It should be possible to remove a number of these accessors once all
    // drag logic is in WebCore.
    void setDidInitiateDrag(bool initiated) { m_didInitiateDrag = initiated; }
    bool didInitiateDrag() const { return m_didInitiateDrag; }
    OptionSet<DragOperation> sourceDragOperationMask() const { return m_sourceDragOperationMask; }
    const URL& draggingImageURL() const { return m_draggingImageURL; }
    void setDragOffset(const IntPoint& offset) { m_dragOffset = offset; }
    const IntPoint& dragOffset() const { return m_dragOffset; }
    OptionSet<DragSourceAction> dragSourceAction() const { return m_dragSourceAction; }
    DragHandlingMethod dragHandlingMethod() const { return m_dragHandlingMethod; }

    Document* documentUnderMouse() const { return m_documentUnderMouse.get(); }
    OptionSet<DragDestinationAction> dragDestinationActionMask() const { return m_dragDestinationActionMask; }
    OptionSet<DragSourceAction> delegateDragSourceAction(const IntPoint& rootViewPoint);

    Element* draggableElement(const Frame*, Element* start, const IntPoint&, DragState&) const;
    WEBCORE_EXPORT void dragEnded();

    WEBCORE_EXPORT void placeDragCaret(const IntPoint&);

    const Vector<Ref<HTMLImageElement>>& droppedImagePlaceholders() const { return m_droppedImagePlaceholders; }
    const Optional<SimpleRange>& droppedImagePlaceholderRange() const { return m_droppedImagePlaceholderRange; }

    WEBCORE_EXPORT void finalizeDroppedImagePlaceholder(HTMLImageElement&);
    WEBCORE_EXPORT void insertDroppedImagePlaceholdersAtCaret(const Vector<IntSize>& imageSizes);

    void prepareForDragStart(Frame& sourceFrame, OptionSet<DragSourceAction>, Element& sourceElement, DataTransfer&, const IntPoint& dragOrigin) const;
    bool startDrag(Frame& src, const DragState&, OptionSet<DragOperation>, const PlatformMouseEvent& dragEvent, const IntPoint& dragOrigin, HasNonDefaultPasteboardData);
    static const IntSize& maxDragImageSize();

    static const int MaxOriginalImageArea;
    static const int DragIconRightInset;
    static const int DragIconBottomInset;
    static const float DragImageAlpha;

private:
    void updateSupportedTypeIdentifiersForDragHandlingMethod(DragHandlingMethod, const DragData&) const;
    bool dispatchTextInputEventFor(Frame*, const DragData&);
    bool canProcessDrag(const DragData&);
    bool concludeEditDrag(const DragData&);
    Optional<DragOperation> dragEnteredOrUpdated(const DragData&);
    Optional<DragOperation> operationForLoad(const DragData&);
    DragHandlingMethod tryDocumentDrag(const DragData&, OptionSet<DragDestinationAction>, Optional<DragOperation>&);
    bool tryDHTMLDrag(const DragData&, Optional<DragOperation>&);
    Optional<DragOperation> dragOperation(const DragData&);
    void clearDragCaret();
    bool dragIsMove(FrameSelection&, const DragData&);
    bool isCopyKeyDown(const DragData&);

    void mouseMovedIntoDocument(Document*);
    bool shouldUseCachedImageForDragImage(const Image&) const;

    Optional<HitTestResult> hitTestResultForDragStart(Frame&, Element&, const IntPoint&) const;

    void doImageDrag(Element&, const IntPoint&, const IntRect&, Frame&, IntPoint&, const DragState&, PromisedAttachmentInfo&&);
    void doSystemDrag(DragImage, const IntPoint&, const IntPoint&, Frame&, const DragState&, PromisedAttachmentInfo&&);

    void beginDrag(DragItem, Frame&, const IntPoint& mouseDownPoint, const IntPoint& mouseDraggedPoint, DataTransfer&, DragSourceAction);

    bool canLoadDataFromDraggingPasteboard() const
    {
#if PLATFORM(IOS_FAMILY)
        return m_isPerformingDrop;
#else
        return true;
#endif
    }

    DragClient& client() const { return *m_client; }

    bool tryToUpdateDroppedImagePlaceholders(const DragData&);
    void removeAllDroppedImagePlaceholders();

    void cleanupAfterSystemDrag();
    void declareAndWriteDragImage(DataTransfer&, Element&, const URL&, const String& label);

    Page& m_page;
    std::unique_ptr<DragClient> m_client;

    RefPtr<Document> m_documentUnderMouse; // The document the mouse was last dragged over.
    RefPtr<Document> m_dragInitiator; // The Document (if any) that initiated the drag.
    RefPtr<HTMLInputElement> m_fileInputElementUnderMouse;
    unsigned m_numberOfItemsToBeAccepted { 0 };
    DragHandlingMethod m_dragHandlingMethod { DragHandlingMethod::None };

    OptionSet<DragDestinationAction> m_dragDestinationActionMask;
    OptionSet<DragSourceAction> m_dragSourceAction;
    bool m_didInitiateDrag { false };
    OptionSet<DragOperation> m_sourceDragOperationMask; // Set in startDrag when a drag starts from a mouse down within WebKit.
    IntPoint m_dragOffset;
    URL m_draggingImageURL;
    bool m_isPerformingDrop { false };
    Vector<Ref<HTMLImageElement>> m_droppedImagePlaceholders;
    Optional<SimpleRange> m_droppedImagePlaceholderRange;
};

WEBCORE_EXPORT bool isDraggableLink(const Element&);

} // namespace WebCore
