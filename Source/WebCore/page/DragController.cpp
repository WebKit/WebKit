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

#include "config.h"
#include "DragController.h"

#include "HTMLAnchorElement.h"
#include "SVGAElement.h"

#if ENABLE(DRAG_SUPPORT)
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "ColorSerialization.h"
#include "DataTransfer.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DragActions.h"
#include "DragClient.h"
#include "DragData.h"
#include "DragImage.h"
#include "DragState.h"
#include "Editing.h"
#include "Editor.h"
#include "EditorClient.h"
#include "ElementAncestorIterator.h"
#include "EventHandler.h"
#include "File.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "HTMLAttachmentElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLPlugInElement.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "Image.h"
#include "ImageOrientation.h"
#include "MoveSelectionCommand.h"
#include "Page.h"
#include "Pasteboard.h"
#include "PlatformKeyboardEvent.h"
#include "PluginDocument.h"
#include "PluginViewBase.h"
#include "Position.h"
#include "PromisedAttachmentInfo.h"
#include "RenderAttachment.h"
#include "RenderFileUploadControl.h"
#include "RenderImage.h"
#include "RenderView.h"
#include "ReplaceSelectionCommand.h"
#include "ResourceRequest.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SimpleRange.h"
#include "StyleProperties.h"
#include "Text.h"
#include "TextEvent.h"
#include "VisiblePosition.h"
#include "WebContentReader.h"
#include "markup.h"
#include <wtf/SetForScope.h>
#endif

#if ENABLE(DATA_DETECTION)
#include "DataDetection.h"
#endif

#if ENABLE(DATA_INTERACTION)
#include "SelectionRect.h"
#endif

namespace WebCore {

bool isDraggableLink(const Element& element)
{
    if (is<HTMLAnchorElement>(element)) {
        auto& anchorElement = downcast<HTMLAnchorElement>(element);
        if (!anchorElement.isLiveLink())
            return false;
#if ENABLE(DATA_DETECTION)
        return !DataDetection::isDataDetectorURL(anchorElement.href());
#else
        return true;
#endif
    }
    if (is<SVGAElement>(element))
        return element.isLink();
    return false;
}

#if ENABLE(DRAG_SUPPORT)
    
static PlatformMouseEvent createMouseEvent(const DragData& dragData)
{
    bool shiftKey = false;
    bool ctrlKey = false;
    bool altKey = false;
    bool metaKey = false;

    PlatformKeyboardEvent::getCurrentModifierState(shiftKey, ctrlKey, altKey, metaKey);

    return PlatformMouseEvent(dragData.clientPosition(), dragData.globalPosition(),
                              LeftButton, PlatformEvent::MouseMoved, 0, shiftKey, ctrlKey, altKey,
                              metaKey, WallTime::now(), ForceAtClick, NoTap);
}

DragController::DragController(Page& page, std::unique_ptr<DragClient>&& client)
    : m_page(page)
    , m_client(WTFMove(client))
{
}

DragController::~DragController() = default;

static RefPtr<DocumentFragment> documentFragmentFromDragData(const DragData& dragData, Frame& frame, Range& context, bool allowPlainText, bool& chosePlainText)
{
    chosePlainText = false;

    Document& document = context.ownerDocument();
    if (dragData.containsCompatibleContent()) {
        if (auto fragment = frame.editor().webContentFromPasteboard(*Pasteboard::createForDragAndDrop(dragData), context, allowPlainText, chosePlainText))
            return fragment;

        if (dragData.containsURL(DragData::DoNotConvertFilenames)) {
            String title;
            String url = dragData.asURL(DragData::DoNotConvertFilenames, &title);
            if (!url.isEmpty()) {
                auto anchor = HTMLAnchorElement::create(document);
                anchor->setHref(url);
                if (title.isEmpty()) {
                    // Try the plain text first because the url might be normalized or escaped.
                    if (dragData.containsPlainText())
                        title = dragData.asPlainText();
                    if (title.isEmpty())
                        title = url;
                }
                anchor->appendChild(document.createTextNode(title));
                auto fragment = document.createDocumentFragment();
                fragment->appendChild(anchor);
                return fragment;
            }
        }
    }
    if (allowPlainText && dragData.containsPlainText()) {
        chosePlainText = true;
        return createFragmentFromText(context, dragData.asPlainText()).ptr();
    }

    return nullptr;
}

#if !PLATFORM(IOS_FAMILY)

DragOperation DragController::platformGenericDragOperation()
{
    return DragOperation::Move;
}

#endif

bool DragController::dragIsMove(FrameSelection& selection, const DragData& dragData)
{
    const VisibleSelection& visibleSelection = selection.selection();
    return m_documentUnderMouse == m_dragInitiator && visibleSelection.isContentEditable() && visibleSelection.isRange() && !isCopyKeyDown(dragData);
}

void DragController::clearDragCaret()
{
    m_page.dragCaretController().clear();
}

void DragController::dragEnded()
{
    m_dragInitiator = nullptr;
    m_didInitiateDrag = false;
    m_documentUnderMouse = nullptr;
    clearDragCaret();
    removeAllDroppedImagePlaceholders();
    
    client().dragEnded();
}

Optional<DragOperation> DragController::dragEntered(const DragData& dragData)
{
    return dragEnteredOrUpdated(dragData);
}

void DragController::dragExited(const DragData& dragData)
{
    auto& mainFrame = m_page.mainFrame();
    if (mainFrame.view())
        mainFrame.eventHandler().cancelDragAndDrop(createMouseEvent(dragData), Pasteboard::createForDragAndDrop(dragData), dragData.draggingSourceOperationMask(), dragData.containsFiles());
    mouseMovedIntoDocument(nullptr);
    if (m_fileInputElementUnderMouse)
        m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(false);
    m_fileInputElementUnderMouse = nullptr;
}

Optional<DragOperation> DragController::dragUpdated(const DragData& dragData)
{
    return dragEnteredOrUpdated(dragData);
}

inline static bool dragIsHandledByDocument(DragHandlingMethod dragHandlingMethod)
{
    return dragHandlingMethod != DragHandlingMethod::None && dragHandlingMethod != DragHandlingMethod::PageLoad;
}

bool DragController::performDragOperation(const DragData& dragData)
{
    if (!m_droppedImagePlaceholders.isEmpty() && m_droppedImagePlaceholderRange && tryToUpdateDroppedImagePlaceholders(dragData)) {
        m_droppedImagePlaceholders.clear();
        m_droppedImagePlaceholderRange = nullptr;
        m_documentUnderMouse = nullptr;
        clearDragCaret();
        return true;
    }

    removeAllDroppedImagePlaceholders();

    SetForScope<bool> isPerformingDrop(m_isPerformingDrop, true);
    IgnoreSelectionChangeForScope ignoreSelectionChanges { m_page.focusController().focusedOrMainFrame() };

    m_documentUnderMouse = m_page.mainFrame().documentAtPoint(dragData.clientPosition());

    ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy = ShouldOpenExternalURLsPolicy::ShouldNotAllow;
    if (m_documentUnderMouse)
        shouldOpenExternalURLsPolicy = m_documentUnderMouse->shouldOpenExternalURLsPolicyToPropagate();

    if ((m_dragDestinationActionMask.contains(DragDestinationAction::DHTML)) && dragIsHandledByDocument(m_dragHandlingMethod)) {
        client().willPerformDragDestinationAction(DragDestinationAction::DHTML, dragData);
        Ref<Frame> mainFrame(m_page.mainFrame());
        bool preventedDefault = false;
        if (mainFrame->view())
            preventedDefault = mainFrame->eventHandler().performDragAndDrop(createMouseEvent(dragData), Pasteboard::createForDragAndDrop(dragData), dragData.draggingSourceOperationMask(), dragData.containsFiles());
        if (preventedDefault) {
            clearDragCaret();
            m_documentUnderMouse = nullptr;
            return true;
        }
    }

    if ((m_dragDestinationActionMask.contains(DragDestinationAction::Edit)) && concludeEditDrag(dragData)) {
        client().didConcludeEditDrag();
        m_documentUnderMouse = nullptr;
        clearDragCaret();
        return true;
    }

    m_documentUnderMouse = nullptr;
    clearDragCaret();

    if (!operationForLoad(dragData))
        return false;

    auto urlString = dragData.asURL();
    if (urlString.isEmpty())
        return false;

    client().willPerformDragDestinationAction(DragDestinationAction::Load, dragData);
    FrameLoadRequest frameLoadRequest { m_page.mainFrame(), ResourceRequest { urlString } };
    frameLoadRequest.setShouldOpenExternalURLsPolicy(shouldOpenExternalURLsPolicy);
    frameLoadRequest.setIsRequestFromClientOrUserInput();
    m_page.mainFrame().loader().load(WTFMove(frameLoadRequest));
    return true;
}

void DragController::mouseMovedIntoDocument(Document* newDocument)
{
    if (m_documentUnderMouse == newDocument)
        return;

    // If we were over another document clear the selection
    if (m_documentUnderMouse)
        clearDragCaret();
    m_documentUnderMouse = newDocument;
}

Optional<DragOperation> DragController::dragEnteredOrUpdated(const DragData& dragData)
{
    mouseMovedIntoDocument(m_page.mainFrame().documentAtPoint(dragData.clientPosition()));

    m_dragDestinationActionMask = dragData.dragDestinationActionMask();
    if (m_dragDestinationActionMask.isEmpty()) {
        clearDragCaret(); // FIXME: Why not call mouseMovedIntoDocument(nullptr)?
        return WTF::nullopt;
    }

    Optional<DragOperation> dragOperation;
    m_dragHandlingMethod = tryDocumentDrag(dragData, m_dragDestinationActionMask, dragOperation);
    if (m_dragHandlingMethod == DragHandlingMethod::None && (m_dragDestinationActionMask.contains(DragDestinationAction::Load))) {
        dragOperation = operationForLoad(dragData);
        if (dragOperation)
            m_dragHandlingMethod = DragHandlingMethod::PageLoad;
    } else if (m_dragHandlingMethod == DragHandlingMethod::SetColor)
        dragOperation = DragOperation::Copy;

    updateSupportedTypeIdentifiersForDragHandlingMethod(m_dragHandlingMethod, dragData);
    return dragOperation;
}

static HTMLInputElement* asFileInput(Node& node)
{
    if (!is<HTMLInputElement>(node))
        return nullptr;

    auto* inputElement = &downcast<HTMLInputElement>(node);

    // If this is a button inside of the a file input, move up to the file input.
    if (inputElement->isTextButton() && is<ShadowRoot>(inputElement->treeScope().rootNode())) {
        auto& host = *downcast<ShadowRoot>(inputElement->treeScope().rootNode()).host();
        inputElement = is<HTMLInputElement>(host) ? &downcast<HTMLInputElement>(host) : nullptr;
    }

    return inputElement && inputElement->isFileUpload() ? inputElement : nullptr;
}

#if ENABLE(INPUT_TYPE_COLOR)

static bool isEnabledColorInput(Node& node)
{
    if (!is<HTMLInputElement>(node))
        return false;
    auto& input = downcast<HTMLInputElement>(node);
    return input.isColorControl() && !input.isDisabledFormControl();
}

static bool isInShadowTreeOfEnabledColorInput(Node& node)
{
    auto* host = node.shadowHost();
    return host && isEnabledColorInput(*host);
}

#endif

// This can return null if an empty document is loaded.
static Element* elementUnderMouse(Document* documentUnderMouse, const IntPoint& p)
{
    Frame* frame = documentUnderMouse->frame();
    float zoomFactor = frame ? frame->pageZoomFactor() : 1;
    LayoutPoint point(p.x() * zoomFactor, p.y() * zoomFactor);

    HitTestResult result(point);
    documentUnderMouse->hitTest(HitTestRequest(), result);

    auto* node = result.innerNode();
    if (!node)
        return nullptr;
    // FIXME: Use parentElementInComposedTree here.
    auto* element = is<Element>(*node) ? &downcast<Element>(*node) : node->parentElement();
    auto* host = element->shadowHost();
    return host ? host : element;
}

#if !ENABLE(DATA_INTERACTION)

void DragController::updateSupportedTypeIdentifiersForDragHandlingMethod(DragHandlingMethod, const DragData&) const
{
}

#endif

DragHandlingMethod DragController::tryDocumentDrag(const DragData& dragData, OptionSet<DragDestinationAction> destinationActionMask, Optional<DragOperation>& dragOperation)
{
    if (!m_documentUnderMouse)
        return DragHandlingMethod::None;

    if (m_dragInitiator && !m_documentUnderMouse->securityOrigin().canReceiveDragData(m_dragInitiator->securityOrigin()))
        return DragHandlingMethod::None;

    bool isHandlingDrag = false;
    if (destinationActionMask.contains(DragDestinationAction::DHTML)) {
        isHandlingDrag = tryDHTMLDrag(dragData, dragOperation);
        // Do not continue if m_documentUnderMouse has been reset by tryDHTMLDrag.
        // tryDHTMLDrag fires dragenter event. The event listener that listens
        // to this event may create a nested message loop (open a modal dialog),
        // which could process dragleave event and reset m_documentUnderMouse in
        // dragExited.
        if (!m_documentUnderMouse)
            return DragHandlingMethod::None;
    }

    // It's unclear why this check is after tryDHTMLDrag.
    // We send drag events in tryDHTMLDrag and that may be the reason.
    RefPtr<FrameView> frameView = m_documentUnderMouse->view();
    if (!frameView)
        return DragHandlingMethod::None;

    if (isHandlingDrag) {
        clearDragCaret();
        m_numberOfItemsToBeAccepted = dragData.numberOfFiles();
        return DragHandlingMethod::NonDefault;
    }

    if (destinationActionMask.contains(DragDestinationAction::Edit) && canProcessDrag(dragData)) {
        if (dragData.containsColor()) {
            dragOperation = DragOperation::Generic;
            return DragHandlingMethod::SetColor;
        }

        IntPoint point = frameView->windowToContents(dragData.clientPosition());
        Element* element = elementUnderMouse(m_documentUnderMouse.get(), point);
        if (!element)
            return DragHandlingMethod::None;
        
        HTMLInputElement* elementAsFileInput = asFileInput(*element);
        if (m_fileInputElementUnderMouse != elementAsFileInput) {
            if (m_fileInputElementUnderMouse)
                m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(false);
            m_fileInputElementUnderMouse = elementAsFileInput;
        }
        
        if (!m_fileInputElementUnderMouse)
            m_page.dragCaretController().setCaretPosition(m_documentUnderMouse->frame()->visiblePositionForPoint(point));
        else
            clearDragCaret();

        Frame* innerFrame = element->document().frame();
        dragOperation = dragIsMove(innerFrame->selection(), dragData) ? DragOperation::Move : DragOperation::Copy;

        unsigned numberOfFiles = dragData.numberOfFiles();
        if (m_fileInputElementUnderMouse) {
            if (m_fileInputElementUnderMouse->isDisabledFormControl())
                m_numberOfItemsToBeAccepted = 0;
            else if (m_fileInputElementUnderMouse->multiple())
                m_numberOfItemsToBeAccepted = numberOfFiles;
            else if (numberOfFiles > 1)
                m_numberOfItemsToBeAccepted = 0;
            else
                m_numberOfItemsToBeAccepted = 1;
            
            if (!m_numberOfItemsToBeAccepted)
                dragOperation = WTF::nullopt;
            m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(m_numberOfItemsToBeAccepted);
        } else {
            // We are not over a file input element. The dragged item(s) will loaded into the view,
            // dropped as text paths on other input elements, or handled by script on the page.
            m_numberOfItemsToBeAccepted = numberOfFiles;
        }

        if (m_fileInputElementUnderMouse)
            return DragHandlingMethod::UploadFile;

        if (m_page.dragCaretController().isContentRichlyEditable())
            return DragHandlingMethod::EditRichText;

        return DragHandlingMethod::EditPlainText;
    }
    
    // We are not over an editable region. Make sure we're clearing any prior drag cursor.
    clearDragCaret();
    if (m_fileInputElementUnderMouse)
        m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(false);
    m_fileInputElementUnderMouse = nullptr;
    return DragHandlingMethod::None;
}

OptionSet<DragSourceAction> DragController::delegateDragSourceAction(const IntPoint& rootViewPoint)
{
    m_dragSourceAction = client().dragSourceActionMaskForPoint(rootViewPoint);
    return m_dragSourceAction;
}

Optional<DragOperation> DragController::operationForLoad(const DragData& dragData)
{
    Document* document = m_page.mainFrame().documentAtPoint(dragData.clientPosition());

    bool pluginDocumentAcceptsDrags = false;

    if (is<PluginDocument>(document)) {
        const Widget* widget = downcast<PluginDocument>(*document).pluginWidget();
        const PluginViewBase* pluginView = is<PluginViewBase>(widget) ? downcast<PluginViewBase>(widget) : nullptr;

        if (pluginView)
            pluginDocumentAcceptsDrags = pluginView->shouldAllowNavigationFromDrags();
    }

    if (document && (m_didInitiateDrag || (is<PluginDocument>(*document) && !pluginDocumentAcceptsDrags) || document->hasEditableStyle()))
        return WTF::nullopt;
    return dragOperation(dragData);
}

static bool setSelectionToDragCaret(Frame* frame, VisibleSelection& dragCaret, const IntPoint& point)
{
    Ref<Frame> protector(*frame);
    frame->selection().setSelection(dragCaret);
    if (frame->selection().selection().isNone()) {
        dragCaret = frame->visiblePositionForPoint(point);
        frame->selection().setSelection(dragCaret);
    }
    return !frame->selection().isNone() && frame->selection().selection().isContentEditable();
}

bool DragController::dispatchTextInputEventFor(Frame* innerFrame, const DragData& dragData)
{
    ASSERT(m_page.dragCaretController().hasCaret());
    String text = m_page.dragCaretController().isContentRichlyEditable() ? emptyString() : dragData.asPlainText();
    Element* target = innerFrame->editor().findEventTargetFrom(m_page.dragCaretController().caretPosition());
    // FIXME: What guarantees target is not null?
    auto event = TextEvent::createForDrop(&innerFrame->windowProxy(), text);
    target->dispatchEvent(event);
    return !event->defaultPrevented();
}

bool DragController::concludeEditDrag(const DragData& dragData)
{
    RefPtr<HTMLInputElement> fileInput = m_fileInputElementUnderMouse;
    if (m_fileInputElementUnderMouse) {
        m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(false);
        m_fileInputElementUnderMouse = nullptr;
    }

    if (!m_documentUnderMouse)
        return false;

    IntPoint point = m_documentUnderMouse->view()->windowToContents(dragData.clientPosition());
    Element* element = elementUnderMouse(m_documentUnderMouse.get(), point);
    if (!element)
        return false;
    RefPtr<Frame> innerFrame = element->document().frame();
    ASSERT(innerFrame);

    if (m_page.dragCaretController().hasCaret() && !dispatchTextInputEventFor(innerFrame.get(), dragData))
        return true;

    if (dragData.containsColor()) {
        Color color = dragData.asColor();
        if (!color.isValid())
            return false;
#if ENABLE(INPUT_TYPE_COLOR)
        if (isEnabledColorInput(*element)) {
            auto& input = downcast<HTMLInputElement>(*element);
            input.setValue(serializationForHTML(color), DispatchInputAndChangeEvent);
            return true;
        }
#endif
        auto innerRange = innerFrame->selection().selection().toNormalizedRange();
        if (!innerRange)
            return false;
        auto style = MutableStyleProperties::create();
        style->setProperty(CSSPropertyColor, serializationForHTML(color), false);
        if (!innerFrame->editor().shouldApplyStyle(style.ptr(), createLiveRange(innerRange).get()))
            return false;
        client().willPerformDragDestinationAction(DragDestinationAction::Edit, dragData);
        innerFrame->editor().applyStyle(style.ptr(), EditAction::SetColor);
        return true;
    }

    if (dragData.containsFiles() && fileInput) {
        // fileInput should be the element we hit tested for, unless it was made
        // display:none in a drop event handler.
        ASSERT(fileInput == element || !fileInput->renderer());
        if (fileInput->isDisabledFormControl())
            return false;

        return fileInput->receiveDroppedFiles(dragData);
    }

    if (!m_page.dragController().canProcessDrag(dragData))
        return false;

    VisibleSelection dragCaret = m_page.dragCaretController().caretPosition();
    auto range = dragCaret.toNormalizedRange();
    RefPtr<Element> rootEditableElement = innerFrame->selection().selection().rootEditableElement();

    // For range to be null a WebKit client must have done something bad while
    // manually controlling drag behaviour
    if (!range)
        return false;

    ResourceCacheValidationSuppressor validationSuppressor(range->start.document().cachedResourceLoader());
    auto& editor = innerFrame->editor();
    bool isMove = dragIsMove(innerFrame->selection(), dragData);
    if (isMove || dragCaret.isContentRichlyEditable()) {
        bool chosePlainText = false;
        RefPtr<DocumentFragment> fragment = documentFragmentFromDragData(dragData, *innerFrame, createLiveRange(*range), true, chosePlainText);
        if (!fragment || !editor.shouldInsertFragment(*fragment, createLiveRange(range).get(), EditorInsertAction::Dropped))
            return false;

        client().willPerformDragDestinationAction(DragDestinationAction::Edit, dragData);

        if (editor.client() && editor.client()->performTwoStepDrop(*fragment, createLiveRange(*range), isMove))
            return true;

        if (isMove) {
            // NSTextView behavior is to always smart delete on moving a selection,
            // but only to smart insert if the selection granularity is word granularity.
            bool smartDelete = editor.smartInsertDeleteEnabled();
            bool smartInsert = smartDelete && innerFrame->selection().granularity() == TextGranularity::WordGranularity && dragData.canSmartReplace();
            MoveSelectionCommand::create(fragment.releaseNonNull(), dragCaret.base(), smartInsert, smartDelete)->apply();
        } else {
            if (setSelectionToDragCaret(innerFrame.get(), dragCaret, point)) {
                OptionSet<ReplaceSelectionCommand::CommandOption> options { ReplaceSelectionCommand::SelectReplacement, ReplaceSelectionCommand::PreventNesting };
                if (dragData.canSmartReplace())
                    options.add(ReplaceSelectionCommand::SmartReplace);
                if (chosePlainText)
                    options.add(ReplaceSelectionCommand::MatchStyle);
                ReplaceSelectionCommand::create(*m_documentUnderMouse, fragment.releaseNonNull(), options, EditAction::InsertFromDrop)->apply();
            }
        }
    } else {
        String text = dragData.asPlainText();
        if (text.isEmpty() || !editor.shouldInsertText(text, createLiveRange(*range).ptr(), EditorInsertAction::Dropped))
            return false;

        client().willPerformDragDestinationAction(DragDestinationAction::Edit, dragData);
        auto fragment = createFragmentFromText(createLiveRange(*range), text);
        if (editor.client() && editor.client()->performTwoStepDrop(fragment.get(), createLiveRange(*range), isMove))
            return true;

        if (setSelectionToDragCaret(innerFrame.get(), dragCaret, point))
            ReplaceSelectionCommand::create(*m_documentUnderMouse, WTFMove(fragment), { ReplaceSelectionCommand::SelectReplacement, ReplaceSelectionCommand::MatchStyle, ReplaceSelectionCommand::PreventNesting }, EditAction::InsertFromDrop)->apply();
    }

    if (rootEditableElement) {
        if (Frame* frame = rootEditableElement->document().frame())
            frame->eventHandler().updateDragStateAfterEditDragIfNeeded(*rootEditableElement);
    }

    return true;
}

bool DragController::canProcessDrag(const DragData& dragData)
{
    IntPoint point = m_page.mainFrame().view()->windowToContents(dragData.clientPosition());
    HitTestResult result = HitTestResult(point);
    if (!m_page.mainFrame().contentRenderer())
        return false;

    constexpr OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::ReadOnly, HitTestRequest::Active, HitTestRequest::AllowChildFrameContent };
    result = m_page.mainFrame().eventHandler().hitTestResultAtPoint(point, hitType);

    auto* dragNode = result.innerNonSharedNode();
    if (!dragNode)
        return false;

    DragData::DraggingPurpose dragPurpose = DragData::DraggingPurpose::ForEditing;
    if (asFileInput(*dragNode))
        dragPurpose = DragData::DraggingPurpose::ForFileUpload;
#if ENABLE(INPUT_TYPE_COLOR)
    else if (isEnabledColorInput(*dragNode) || isInShadowTreeOfEnabledColorInput(*dragNode))
        dragPurpose = DragData::DraggingPurpose::ForColorControl;
#endif

    if (!dragData.containsCompatibleContent(dragPurpose))
        return false;

    if (dragPurpose == DragData::DraggingPurpose::ForFileUpload)
        return true;

#if ENABLE(INPUT_TYPE_COLOR)
    if (dragPurpose == DragData::DraggingPurpose::ForColorControl)
        return true;
#endif

    if (is<HTMLPlugInElement>(*dragNode)) {
        if (!downcast<HTMLPlugInElement>(*dragNode).canProcessDrag() && !dragNode->hasEditableStyle())
            return false;
    } else if (!dragNode->hasEditableStyle())
        return false;

    if (m_didInitiateDrag && m_documentUnderMouse == m_dragInitiator && result.isSelected())
        return false;

    return true;
}

static Optional<DragOperation> defaultOperationForDrag(OptionSet<DragOperation> sourceOperationMask)
{
    // This is designed to match IE's operation fallback for the case where
    // the page calls preventDefault() in a drag event but doesn't set dropEffect.
    if (sourceOperationMask.containsAll({ DragOperation::Copy, DragOperation::Link, DragOperation::Generic, DragOperation::Private, DragOperation::Move, DragOperation::Delete }))
        return DragOperation::Copy;
    if (sourceOperationMask.isEmpty())
        return WTF::nullopt;
    if (sourceOperationMask.contains(DragOperation::Move))
        return DragOperation::Move;
    if (sourceOperationMask.contains(DragOperation::Generic))
        return DragController::platformGenericDragOperation();
    if (sourceOperationMask.contains(DragOperation::Copy))
        return DragOperation::Copy;
    if (sourceOperationMask.contains(DragOperation::Link))
        return DragOperation::Link;
    
    // FIXME: Does IE really return "generic" even if no operations were allowed by the source?
    return DragOperation::Generic;
}

bool DragController::tryDHTMLDrag(const DragData& dragData, Optional<DragOperation>& operation)
{
    ASSERT(m_documentUnderMouse);
    Ref<Frame> mainFrame(m_page.mainFrame());
    RefPtr<FrameView> viewProtector = mainFrame->view();
    if (!viewProtector)
        return false;

    auto sourceOperationMask = dragData.draggingSourceOperationMask();
    auto targetResponse = mainFrame->eventHandler().updateDragAndDrop(createMouseEvent(dragData), [&dragData]() {
        return Pasteboard::createForDragAndDrop(dragData);
    }, sourceOperationMask, dragData.containsFiles());
    if (!targetResponse.accept)
        return false;

    if (!targetResponse.operationMask)
        operation = defaultOperationForDrag(sourceOperationMask);
    else if (!sourceOperationMask.containsAny(targetResponse.operationMask.value())) // The element picked an operation which is not supported by the source.
        operation = WTF::nullopt;
    else
        operation = defaultOperationForDrag(targetResponse.operationMask.value());

    return true;
}

static bool imageElementIsDraggable(const HTMLImageElement& image, const Frame& sourceFrame)
{
    if (sourceFrame.settings().loadsImagesAutomatically())
        return true;

    auto* renderer = image.renderer();
    if (!is<RenderImage>(renderer))
        return false;

    auto* cachedImage = downcast<RenderImage>(*renderer).cachedImage();
    return cachedImage && !cachedImage->errorOccurred() && cachedImage->imageForRenderer(renderer);
}

#if ENABLE(ATTACHMENT_ELEMENT)

static RefPtr<HTMLAttachmentElement> enclosingAttachmentElement(Element& element)
{
    if (is<HTMLAttachmentElement>(element))
        return downcast<HTMLAttachmentElement>(&element);

    if (is<HTMLAttachmentElement>(element.parentOrShadowHostElement()))
        return downcast<HTMLAttachmentElement>(element.parentOrShadowHostElement());

    return { };
}

#endif

Element* DragController::draggableElement(const Frame* sourceFrame, Element* startElement, const IntPoint& dragOrigin, DragState& state) const
{
    state.type = sourceFrame->selection().contains(dragOrigin) ? DragSourceAction::Selection : OptionSet<DragSourceAction>({ });
    if (!startElement)
        return nullptr;
#if ENABLE(ATTACHMENT_ELEMENT)
    if (auto attachment = enclosingAttachmentElement(*startElement)) {
        auto selection = sourceFrame->selection().selection();
        bool isSingleAttachmentSelection = selection.start() == Position(attachment.get(), Position::PositionIsBeforeAnchor) && selection.end() == Position(attachment.get(), Position::PositionIsAfterAnchor);
        bool isAttachmentElementInCurrentSelection = false;
        if (auto selectedRange = selection.toNormalizedRange()) {
            auto compareResult = createLiveRange(*selectedRange)->compareNode(*attachment);
            isAttachmentElementInCurrentSelection = !compareResult.hasException() && compareResult.releaseReturnValue() == Range::NODE_INSIDE;
        }

        if (!isAttachmentElementInCurrentSelection || isSingleAttachmentSelection) {
            state.type = DragSourceAction::Attachment;
            return attachment.get();
        }
    }
#endif

    for (auto* element = startElement; element; element = element->parentOrShadowHostElement()) {
        auto* renderer = element->renderer();
        if (!renderer)
            continue;

        UserDrag dragMode = renderer->style().userDrag();
        if (m_dragSourceAction.contains(DragSourceAction::DHTML) && dragMode == UserDrag::Element) {
            state.type.add(DragSourceAction::DHTML);
            return element;
        }
        if (dragMode == UserDrag::Auto) {
            if ((m_dragSourceAction.contains(DragSourceAction::Image))
                && is<HTMLImageElement>(*element)
                && imageElementIsDraggable(downcast<HTMLImageElement>(*element), *sourceFrame)) {
                state.type.add(DragSourceAction::Image);
                return element;
            }
            if (m_dragSourceAction.contains(DragSourceAction::Link) && isDraggableLink(*element)) {
                state.type.add(DragSourceAction::Link);
                return element;
            }
#if ENABLE(ATTACHMENT_ELEMENT)
            if (m_dragSourceAction.contains(DragSourceAction::Attachment)
                && is<HTMLAttachmentElement>(*element)
                && downcast<HTMLAttachmentElement>(*element).file()) {
                state.type.add(DragSourceAction::Attachment);
                return element;
            }
#endif
#if ENABLE(INPUT_TYPE_COLOR)
            if (m_dragSourceAction.contains(DragSourceAction::Color) && isEnabledColorInput(*element)) {
                state.type.add(DragSourceAction::Color);
                return element;
            }
#endif
        }
    }

    // We either have nothing to drag or we have a selection and we're not over a draggable element.
    if (state.type.contains(DragSourceAction::Selection) && m_dragSourceAction.contains(DragSourceAction::Selection))
        return startElement;

    return nullptr;
}

static CachedImage* getCachedImage(Element& element)
{
    RenderObject* renderer = element.renderer();
    if (!is<RenderImage>(renderer))
        return nullptr;
    auto& image = downcast<RenderImage>(*renderer);
    return image.cachedImage();
}

static Image* getImage(Element& element)
{
    CachedImage* cachedImage = getCachedImage(element);
    // Don't use cachedImage->imageForRenderer() here as that may return BitmapImages for cached SVG Images.
    // Users of getImage() want access to the SVGImage, in order to figure out the filename extensions,
    // which would be empty when asking the cached BitmapImages.
    return (cachedImage && !cachedImage->errorOccurred()) ?
        cachedImage->image() : nullptr;
}

static void selectElement(Element& element)
{
    RefPtr<Range> range = element.document().createRange();
    range->selectNode(element);
    element.document().frame()->selection().setSelection(VisibleSelection(*range, DOWNSTREAM));
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

static FloatPoint dragImageAnchorPointForSelectionDrag(Frame& frame, const IntPoint& mouseDraggedPoint)
{
    IntRect draggingRect = enclosingIntRect(frame.selection().selectionBounds());

    float x = (mouseDraggedPoint.x() - draggingRect.x()) / (float)draggingRect.width();
    float y = (mouseDraggedPoint.y() - draggingRect.y()) / (float)draggingRect.height();

    return FloatPoint { x, y };
}

static IntPoint dragLocForSelectionDrag(Frame& src)
{
    IntRect draggingRect = enclosingIntRect(src.selection().selectionBounds());
    int xpos = draggingRect.maxX();
    xpos = draggingRect.x() < xpos ? draggingRect.x() : xpos;
    int ypos = draggingRect.maxY();
#if PLATFORM(COCOA)
    // Deal with flipped coordinates on Mac
    ypos = draggingRect.y() > ypos ? draggingRect.y() : ypos;
#else
    ypos = draggingRect.y() < ypos ? draggingRect.y() : ypos;
#endif
    return IntPoint(xpos, ypos);
}

void DragController::prepareForDragStart(Frame& source, OptionSet<DragSourceAction> actionMask, Element& element, DataTransfer& dataTransfer, const IntPoint& dragOrigin) const
{
#if !PLATFORM(WIN)
    Ref<Frame> protector(source);
    auto hitTestResult = hitTestResultForDragStart(source, element, dragOrigin);
    if (!hitTestResult)
        return;

    auto& pasteboard = dataTransfer.pasteboard();
    auto& editor = source.editor();
    if (actionMask == DragSourceAction::Selection) {
        if (enclosingTextFormControl(source.selection().selection().start()))
            pasteboard.writePlainText(editor.selectedTextForDataTransfer(), Pasteboard::CannotSmartReplace);
        else
            editor.writeSelectionToPasteboard(pasteboard);
        return;
    }

    auto* image = getImage(element);
    auto imageURL = hitTestResult->absoluteImageURL();
    if (actionMask.contains(DragSourceAction::Image) && !imageURL.isEmpty() && image && !image->isNull()) {
        editor.writeImageToPasteboard(pasteboard, element, imageURL, { });
        return;
    }

    auto linkURL = hitTestResult->absoluteLinkURL();
    if (actionMask.contains(DragSourceAction::Link) && !linkURL.isEmpty() && source.document()->securityOrigin().canDisplay(linkURL))
        editor.copyURL(linkURL, hitTestResult->textContent().simplifyWhiteSpace(), pasteboard);
#else
    // FIXME: Make this work on Windows by implementing Editor::writeSelectionToPasteboard and Editor::writeImageToPasteboard.
    UNUSED_PARAM(source);
    UNUSED_PARAM(actionMask);
    UNUSED_PARAM(element);
    UNUSED_PARAM(dataTransfer);
    UNUSED_PARAM(dragOrigin);
#endif
}

Optional<HitTestResult> DragController::hitTestResultForDragStart(Frame& source, Element& element, const IntPoint& location) const
{
    if (!source.view() || !source.contentRenderer())
        return WTF::nullopt;

    constexpr OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::ReadOnly, HitTestRequest::Active, HitTestRequest::AllowChildFrameContent };
    auto hitTestResult = source.eventHandler().hitTestResultAtPoint(location, hitType);

    bool sourceContainsHitNode = element.containsIncludingShadowDOM(hitTestResult.innerNode());
    if (!sourceContainsHitNode) {
        // The original node being dragged isn't under the drag origin anymore... maybe it was
        // hidden or moved out from under the cursor. Regardless, we don't want to start a drag on
        // something that's not actually under the drag origin.
        return WTF::nullopt;
    }

    return { hitTestResult };
}

bool DragController::startDrag(Frame& src, const DragState& state, OptionSet<DragOperation> sourceOperationMask, const PlatformMouseEvent& dragEvent, const IntPoint& dragOrigin, HasNonDefaultPasteboardData hasData)
{
    if (!state.source)
        return false;

    Ref<Frame> protector(src);
    auto hitTestResult = hitTestResultForDragStart(src, *state.source, dragOrigin);
    if (!hitTestResult)
        return false;

    auto linkURL = hitTestResult->absoluteLinkURL();
    auto imageURL = hitTestResult->absoluteImageURL();

    IntPoint mouseDraggedPoint = src.view()->windowToContents(dragEvent.position());

    m_draggingImageURL = URL();
    m_sourceDragOperationMask = sourceOperationMask;

    DragImage dragImage;
    IntPoint dragLoc(0, 0);
    IntPoint dragImageOffset(0, 0);

    ASSERT(state.dataTransfer);

    DataTransfer& dataTransfer = *state.dataTransfer;
    if (state.type == DragSourceAction::DHTML) {
        dragImage = DragImage { dataTransfer.createDragImage(dragImageOffset) };
        // We allow DHTML/JS to set the drag image, even if its a link, image or text we're dragging.
        // This is in the spirit of the IE API, which allows overriding of pasteboard data and DragOp.
        if (dragImage) {
            dragLoc = dragLocForDHTMLDrag(mouseDraggedPoint, dragOrigin, dragImageOffset, !linkURL.isEmpty());
            m_dragOffset = dragImageOffset;
        }
    }

    if (state.type == DragSourceAction::Selection || !imageURL.isEmpty() || !linkURL.isEmpty()) {
        // Selection, image, and link drags receive a default set of allowed drag operations that
        // follows from:
        // http://trac.webkit.org/browser/trunk/WebKit/mac/WebView/WebHTMLView.mm?rev=48526#L3430
        m_sourceDragOperationMask.add({ DragOperation::Generic, DragOperation::Copy });
    }

    ASSERT(state.source);
    Element& element = *state.source;

    bool mustUseLegacyDragClient = hasData == HasNonDefaultPasteboardData::Yes || client().useLegacyDragClient();

    IntRect dragImageBounds;
    Image* image = getImage(element);
    if (state.type == DragSourceAction::Selection) {
        PasteboardWriterData pasteboardWriterData;

        if (hasData == HasNonDefaultPasteboardData::No) {
            if (src.selection().selection().isNone()) {
                // The page may have cleared out the selection in the dragstart handler, in which case we should bail
                // out of the drag, since there is no content to write to the pasteboard.
                return false;
            }

            // FIXME: This entire block is almost identical to the code in Editor::copy, and the code should be shared.
            auto selectionRange = src.selection().selection().toNormalizedRange();
            ASSERT(selectionRange);

            src.editor().willWriteSelectionToPasteboard(createLiveRange(*selectionRange).ptr());

            if (enclosingTextFormControl(src.selection().selection().start())) {
                if (mustUseLegacyDragClient)
                    dataTransfer.pasteboard().writePlainText(src.editor().selectedTextForDataTransfer(), Pasteboard::CannotSmartReplace);
                else {
                    PasteboardWriterData::PlainText plainText;
                    plainText.canSmartCopyOrDelete = false;
                    plainText.text = src.editor().selectedTextForDataTransfer();
                    pasteboardWriterData.setPlainText(WTFMove(plainText));
                }
            } else {
                if (mustUseLegacyDragClient) {
#if !PLATFORM(WIN)
                    src.editor().writeSelectionToPasteboard(dataTransfer.pasteboard());
#else
                    // FIXME: Convert Windows to match the other platforms and delete this.
                    dataTransfer.pasteboard().writeSelection(createLiveRange(*selectionRange), src.editor().canSmartCopyOrDelete(), src, IncludeImageAltTextForDataTransfer);
#endif
                } else {
#if PLATFORM(COCOA)
                    src.editor().writeSelection(pasteboardWriterData);
#endif
                }
            }

            src.editor().didWriteSelectionToPasteboard();
        }
        client().willPerformDragSourceAction(DragSourceAction::Selection, dragOrigin, dataTransfer);
        if (!dragImage) {
            TextIndicatorData textIndicator;
            dragImage = DragImage { dissolveDragImageToFraction(createDragImageForSelection(src, textIndicator), DragImageAlpha) };
            if (textIndicator.contentImage)
                dragImage.setIndicatorData(textIndicator);
            dragLoc = dragLocForSelectionDrag(src);
            m_dragOffset = IntPoint(dragOrigin.x() - dragLoc.x(), dragOrigin.y() - dragLoc.y());
        }

        if (!dragImage)
            return false;

        if (mustUseLegacyDragClient) {
            doSystemDrag(WTFMove(dragImage), dragLoc, dragOrigin, src, state, { });
            return true;
        }

        DragItem dragItem;
        dragItem.imageAnchorPoint = dragImageAnchorPointForSelectionDrag(src, mouseDraggedPoint);
        dragItem.image = WTFMove(dragImage);
        dragItem.data = WTFMove(pasteboardWriterData);

        beginDrag(WTFMove(dragItem), src, dragOrigin, mouseDraggedPoint, dataTransfer, DragSourceAction::Selection);

        return true;
    }

    if (!src.document()->securityOrigin().canDisplay(linkURL)) {
        src.document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Not allowed to drag local resource: " + linkURL.stringCenterEllipsizedToLength());
        return false;
    }

    if (!imageURL.isEmpty() && image && !image->isNull() && m_dragSourceAction.contains(DragSourceAction::Image)) {
        // We shouldn't be starting a drag for an image that can't provide an extension.
        // This is an early detection for problems encountered later upon drop.
        ASSERT(!image->filenameExtension().isEmpty());

#if ENABLE(ATTACHMENT_ELEMENT)
        auto attachmentInfo = src.editor().promisedAttachmentInfo(element);
#else
        PromisedAttachmentInfo attachmentInfo;
#endif

        if (hasData == HasNonDefaultPasteboardData::No) {
            m_draggingImageURL = imageURL;
            if (element.isContentRichlyEditable())
                selectElement(element);
            if (!attachmentInfo)
                declareAndWriteDragImage(dataTransfer, element, !linkURL.isEmpty() ? linkURL : imageURL, hitTestResult->altDisplayString());
        }

        client().willPerformDragSourceAction(DragSourceAction::Image, dragOrigin, dataTransfer);

        if (!dragImage)
            doImageDrag(element, dragOrigin, hitTestResult->imageRect(), src, m_dragOffset, state, WTFMove(attachmentInfo));
        else {
            // DHTML defined drag image
            doSystemDrag(WTFMove(dragImage), dragLoc, dragOrigin, src, state, WTFMove(attachmentInfo));
        }

        return true;
    }

    if (!linkURL.isEmpty() && m_dragSourceAction.contains(DragSourceAction::Link)) {
        PasteboardWriterData pasteboardWriterData;

        String textContentWithSimplifiedWhiteSpace = hitTestResult->textContent().simplifyWhiteSpace();

        if (hasData == HasNonDefaultPasteboardData::No) {
            // Simplify whitespace so the title put on the dataTransfer resembles what the user sees
            // on the web page. This includes replacing newlines with spaces.
            if (mustUseLegacyDragClient)
                src.editor().copyURL(linkURL, textContentWithSimplifiedWhiteSpace, dataTransfer.pasteboard());
            else
                pasteboardWriterData.setURLData(src.editor().pasteboardWriterURL(linkURL, textContentWithSimplifiedWhiteSpace));
        } else if (dataTransfer.pasteboard().canWriteTrustworthyWebURLsPboardType()) {
            // Make sure the pasteboard also contains trustworthy link data
            // but don't overwrite more general pasteboard types.
            PasteboardURL pasteboardURL;
            pasteboardURL.url = linkURL;
            pasteboardURL.title = hitTestResult->textContent();
            dataTransfer.pasteboard().writeTrustworthyWebURLsPboardType(pasteboardURL);
        }

        const VisibleSelection& sourceSelection = src.selection().selection();
        if (sourceSelection.isCaret() && sourceSelection.isContentEditable()) {
            // a user can initiate a drag on a link without having any text
            // selected.  In this case, we should expand the selection to
            // the enclosing anchor element
            Position pos = sourceSelection.base();
            Node* node = enclosingAnchorElement(pos);
            if (node)
                src.selection().setSelection(VisibleSelection::selectionFromContentsOfNode(node));
        }

        client().willPerformDragSourceAction(DragSourceAction::Link, dragOrigin, dataTransfer);
        if (!dragImage) {
            TextIndicatorData textIndicator;
            dragImage = DragImage { createDragImageForLink(element, linkURL, textContentWithSimplifiedWhiteSpace, textIndicator, src.settings().fontRenderingMode(), m_page.deviceScaleFactor()) };
            if (dragImage) {
                m_dragOffset = dragOffsetForLinkDragImage(dragImage.get());
                dragLoc = IntPoint(dragOrigin.x() + m_dragOffset.x(), dragOrigin.y() + m_dragOffset.y());
                dragImage = DragImage { platformAdjustDragImageForDeviceScaleFactor(dragImage.get(), m_page.deviceScaleFactor()) };
                if (textIndicator.contentImage)
                    dragImage.setIndicatorData(textIndicator);
            }
        }

        if (mustUseLegacyDragClient) {
            doSystemDrag(WTFMove(dragImage), dragLoc, dragOrigin, src, state, { });
            return true;
        }

        DragItem dragItem;
        dragItem.imageAnchorPoint = dragImage ? anchorPointForLinkDragImage(dragImage.get()) : FloatPoint();
        dragItem.image = WTFMove(dragImage);
        dragItem.data = WTFMove(pasteboardWriterData);

        beginDrag(WTFMove(dragItem), src, dragOrigin, mouseDraggedPoint, dataTransfer, DragSourceAction::Selection);

        return true;
    }

#if ENABLE(ATTACHMENT_ELEMENT)
    if (is<HTMLAttachmentElement>(element) && m_dragSourceAction.contains(DragSourceAction::Attachment)) {
        auto& attachment = downcast<HTMLAttachmentElement>(element);
        auto* attachmentRenderer = attachment.renderer();

        src.editor().setIgnoreSelectionChanges(true);
        auto previousSelection = src.selection().selection();
        selectElement(element);

        PromisedAttachmentInfo promisedAttachment;
        if (hasData == HasNonDefaultPasteboardData::No) {
            auto& editor = src.editor();
            promisedAttachment = editor.promisedAttachmentInfo(attachment);
#if PLATFORM(COCOA)
            if (!promisedAttachment && editor.client()) {
                // Otherwise, if no file URL is specified, call out to the injected bundle to populate the pasteboard with data.
                editor.willWriteSelectionToPasteboard(createLiveRange(src.selection().selection().toNormalizedRange()).get());
                editor.writeSelectionToPasteboard(dataTransfer.pasteboard());
                editor.didWriteSelectionToPasteboard();
            }
#endif
        }
        
        client().willPerformDragSourceAction(DragSourceAction::Attachment, dragOrigin, dataTransfer);
        
        if (!dragImage) {
            TextIndicatorData textIndicator;
            if (attachmentRenderer)
                attachmentRenderer->setShouldDrawBorder(false);
            dragImage = DragImage { dissolveDragImageToFraction(createDragImageForSelection(src, textIndicator), DragImageAlpha) };
            if (attachmentRenderer)
                attachmentRenderer->setShouldDrawBorder(true);
            if (textIndicator.contentImage)
                dragImage.setIndicatorData(textIndicator);
            dragLoc = dragLocForSelectionDrag(src);
            m_dragOffset = IntPoint(dragOrigin.x() - dragLoc.x(), dragOrigin.y() - dragLoc.y());
        }
        doSystemDrag(WTFMove(dragImage), dragLoc, dragOrigin, src, state, WTFMove(promisedAttachment));
        if (!element.isContentRichlyEditable())
            src.selection().setSelection(previousSelection);
        src.editor().setIgnoreSelectionChanges(false);
        return true;
    }
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    bool isColorControl = is<HTMLInputElement>(state.source) && downcast<HTMLInputElement>(*state.source).isColorControl();
    if (isColorControl && m_dragSourceAction.contains(DragSourceAction::Color)) {
        auto& input = downcast<HTMLInputElement>(*state.source);
        auto color = input.valueAsColor();

        Path visiblePath;
        dragImage = DragImage { createDragImageForColor(color, input.boundsInRootViewSpace(), input.document().page()->pageScaleFactor(), visiblePath) };
        dragImage.setVisiblePath(visiblePath);
        dataTransfer.pasteboard().write(color);
        dragImageOffset = IntPoint { dragImageSize(dragImage.get()) };
        dragLoc = dragLocForDHTMLDrag(mouseDraggedPoint, dragOrigin, dragImageOffset, false);

        client().willPerformDragSourceAction(DragSourceAction::Color, dragOrigin, dataTransfer);
        doSystemDrag(WTFMove(dragImage), dragLoc, dragOrigin, src, state, { });
        return true;
    }
#endif

    if (state.type == DragSourceAction::DHTML && dragImage) {
        ASSERT(m_dragSourceAction.contains(DragSourceAction::DHTML));
        client().willPerformDragSourceAction(DragSourceAction::DHTML, dragOrigin, dataTransfer);
        doSystemDrag(WTFMove(dragImage), dragLoc, dragOrigin, src, state, { });
        return true;
    }

    return false;
}

void DragController::doImageDrag(Element& element, const IntPoint& dragOrigin, const IntRect& layoutRect, Frame& frame, IntPoint& dragImageOffset, const DragState& state, PromisedAttachmentInfo&& attachmentInfo)
{
    IntPoint mouseDownPoint = dragOrigin;
    DragImage dragImage;
    IntPoint scaledOrigin;

    if (!element.renderer())
        return;

    ImageOrientation orientation = element.renderer()->imageOrientation();

    Image* image = getImage(element);
    if (image && !layoutRect.isEmpty() && shouldUseCachedImageForDragImage(*image) && (dragImage = DragImage { createDragImageFromImage(image, orientation) })) {
        dragImage = DragImage { fitDragImageToMaxSize(dragImage.get(), layoutRect.size(), maxDragImageSize()) };
        IntSize fittedSize = dragImageSize(dragImage.get());

        dragImage = DragImage { platformAdjustDragImageForDeviceScaleFactor(dragImage.get(), m_page.deviceScaleFactor()) };
        dragImage = DragImage { dissolveDragImageToFraction(dragImage.get(), DragImageAlpha) };

        // Properly orient the drag image and orient it differently if it's smaller than the original.
        float scale = fittedSize.width() / (float)layoutRect.width();
        float dx = scale * (layoutRect.x() - mouseDownPoint.x());
        float originY = layoutRect.y();
#if PLATFORM(COCOA)
        // Compensate for accursed flipped coordinates in Cocoa.
        originY += layoutRect.height();
#endif
        float dy = scale * (originY - mouseDownPoint.y());
        scaledOrigin = IntPoint((int)(dx + 0.5), (int)(dy + 0.5));
    } else {
        if (CachedImage* cachedImage = getCachedImage(element)) {
            dragImage = DragImage { createDragImageIconForCachedImageFilename(cachedImage->response().suggestedFilename()) };
            if (dragImage) {
                dragImage = DragImage { platformAdjustDragImageForDeviceScaleFactor(dragImage.get(), m_page.deviceScaleFactor()) };
                scaledOrigin = IntPoint(DragIconRightInset - dragImageSize(dragImage.get()).width(), DragIconBottomInset);
            }
        }
    }

    if (!dragImage)
        return;

    dragImageOffset = mouseDownPoint + scaledOrigin;
    doSystemDrag(WTFMove(dragImage), dragImageOffset, dragOrigin, frame, state, WTFMove(attachmentInfo));
}

void DragController::beginDrag(DragItem dragItem, Frame& frame, const IntPoint& mouseDownPoint, const IntPoint& mouseDraggedPoint, DataTransfer& dataTransfer, DragSourceAction dragSourceAction)
{
    ASSERT(!client().useLegacyDragClient());

    m_didInitiateDrag = true;
    m_dragInitiator = frame.document();

    // Protect this frame and view, as a load may occur mid drag and attempt to unload this frame
    Ref<Frame> mainFrameProtector(m_page.mainFrame());
    RefPtr<FrameView> viewProtector = mainFrameProtector->view();

    auto mouseDownPointInRootViewCoordinates = viewProtector->rootViewToContents(frame.view()->contentsToRootView(mouseDownPoint));
    auto mouseDraggedPointInRootViewCoordinates = viewProtector->rootViewToContents(frame.view()->contentsToRootView(mouseDraggedPoint));

    client().beginDrag(WTFMove(dragItem), frame, mouseDownPointInRootViewCoordinates, mouseDraggedPointInRootViewCoordinates, dataTransfer, dragSourceAction);
}

static RefPtr<Element> containingLinkElement(Element& element)
{
    for (auto& currentElement : lineageOfType<Element>(element)) {
        if (currentElement.isLink())
            return &currentElement;
    }
    return nullptr;
}

void DragController::doSystemDrag(DragImage image, const IntPoint& dragLoc, const IntPoint& eventPos, Frame& frame, const DragState& state, PromisedAttachmentInfo&& promisedAttachmentInfo)
{
    m_didInitiateDrag = true;
    m_dragInitiator = frame.document();
    // Protect this frame and view, as a load may occur mid drag and attempt to unload this frame
    Ref<Frame> frameProtector(m_page.mainFrame());
    RefPtr<FrameView> viewProtector = frameProtector->view();

    DragItem item;
    item.image = WTFMove(image);
    ASSERT(state.type.hasExactlyOneBitSet());
    item.sourceAction = state.type.toSingleValue();
    item.promisedAttachmentInfo = WTFMove(promisedAttachmentInfo);

    auto eventPositionInRootViewCoordinates = frame.view()->contentsToRootView(eventPos);
    auto dragLocationInRootViewCoordinates = frame.view()->contentsToRootView(dragLoc);
    item.eventPositionInContentCoordinates = viewProtector->rootViewToContents(eventPositionInRootViewCoordinates);
    item.dragLocationInContentCoordinates = viewProtector->rootViewToContents(dragLocationInRootViewCoordinates);
    item.dragLocationInWindowCoordinates = viewProtector->contentsToWindow(item.dragLocationInContentCoordinates);
    if (auto element = state.source) {
        auto dataTransferImageElement = state.dataTransfer->dragImageElement();
        if (state.type == DragSourceAction::DHTML) {
            // If the drag image has been customized, fall back to positioning the preview relative to the drag event location.
            IntSize dragPreviewSize;
            if (dataTransferImageElement)
                dragPreviewSize = dataTransferImageElement->boundsInRootViewSpace().size();
            else {
                dragPreviewSize = dragImageSize(item.image.get());
                if (auto* page = frame.page())
                    dragPreviewSize.scale(1 / page->deviceScaleFactor());
            }
            item.dragPreviewFrameInRootViewCoordinates = { dragLocationInRootViewCoordinates, WTFMove(dragPreviewSize) };
        } else {
            // We can position the preview using the bounds of the drag source element.
            item.dragPreviewFrameInRootViewCoordinates = element->boundsInRootViewSpace();
        }

        if (auto link = containingLinkElement(*element)) {
            auto titleAttribute = link->attributeWithoutSynchronization(HTMLNames::titleAttr);
            item.title = titleAttribute.isEmpty() ? link->innerText() : titleAttribute.string();
            item.url = frame.document()->completeURL(stripLeadingAndTrailingHTMLSpaces(link->getAttribute(HTMLNames::hrefAttr)));
        }
    }
    client().startDrag(WTFMove(item), *state.dataTransfer, frameProtector.get());
    // DragClient::startDrag can cause our Page to dispear, deallocating |this|.
    if (!frameProtector->page())
        return;

    cleanupAfterSystemDrag();
}

void DragController::removeAllDroppedImagePlaceholders()
{
    m_droppedImagePlaceholderRange = nullptr;
    for (auto& placeholder : std::exchange(m_droppedImagePlaceholders, { })) {
        if (placeholder->isConnected())
            placeholder->remove();
    }
}

bool DragController::tryToUpdateDroppedImagePlaceholders(const DragData& dragData)
{
    ASSERT(!m_droppedImagePlaceholders.isEmpty());
    ASSERT(m_droppedImagePlaceholderRange);

    auto document = makeRef(m_droppedImagePlaceholders[0]->document());
    auto frame = makeRefPtr(document->frame());
    if (!frame)
        return false;

    WebContentReader reader(*frame, *m_droppedImagePlaceholderRange, true);
    auto pasteboard = Pasteboard::createForDragAndDrop(dragData);
    pasteboard->read(reader);

    if (!reader.fragment)
        return false;

    Vector<Ref<HTMLImageElement>> imageElements;
    for (auto& imageElement : descendantsOfType<HTMLImageElement>(*reader.fragment))
        imageElements.append(imageElement);

    if (imageElements.size() != m_droppedImagePlaceholders.size()) {
        ASSERT_NOT_REACHED();
        return false;
    }

    for (size_t i = 0; i < imageElements.size(); ++i) {
        auto& imageElement = imageElements[i];
        auto& placeholder = m_droppedImagePlaceholders[i];
        placeholder->setAttributeWithoutSynchronization(HTMLNames::srcAttr, imageElement->attributeWithoutSynchronization(HTMLNames::srcAttr));
#if ENABLE(ATTACHMENT_ELEMENT)
        if (auto attachment = imageElement->attachmentElement())
            placeholder->setAttachmentElement(attachment.releaseNonNull());
#endif
    }
    return true;
}

void DragController::insertDroppedImagePlaceholdersAtCaret(const Vector<IntSize>& imageSizes)
{
    auto& caretController = m_page.dragCaretController();
    if (!caretController.isContentRichlyEditable())
        return;

    auto dropCaret = caretController.caretPosition();
    if (dropCaret.isNull())
        return;

    auto document = makeRefPtr(dropCaret.deepEquivalent().document());
    if (!document)
        return;

    auto frame = makeRefPtr(document->frame());
    if (!frame)
        return;

    IgnoreSelectionChangeForScope selectionChange { *frame };

    auto fragment = DocumentFragment::create(*document);
    for (auto& size : imageSizes) {
        ASSERT(!size.isEmpty());
        auto image = HTMLImageElement::create(*document);
        image->setAttributeWithoutSynchronization(HTMLNames::widthAttr, AtomString::number(size.width()));
        image->setAttributeWithoutSynchronization(HTMLNames::heightAttr, AtomString::number(size.height()));
        image->setInlineStyleProperty(CSSPropertyMaxWidth, 100, CSSUnitType::CSS_PERCENTAGE);
        image->setInlineStyleProperty(CSSPropertyBackgroundColor, serializationForCSS(Color::black.colorWithAlpha(13)));
        image->setIsDroppedImagePlaceholder();
        fragment->appendChild(WTFMove(image));
    }

    frame->selection().setSelection(dropCaret);

    auto command = ReplaceSelectionCommand::create(*document, WTFMove(fragment), { ReplaceSelectionCommand::PreventNesting, ReplaceSelectionCommand::SmartReplace }, EditAction::InsertFromDrop);
    command->apply();

    auto insertedContentRange = command->insertedContentRange();
    if (!insertedContentRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto container = makeRefPtr(insertedContentRange->commonAncestorContainer());
    if (!is<ContainerNode>(container)) {
        ASSERT_NOT_REACHED();
        return;
    }

    Vector<Ref<HTMLImageElement>> placeholders;
    for (auto& placeholder : descendantsOfType<HTMLImageElement>(downcast<ContainerNode>(*container))) {
        auto intersectsNode = insertedContentRange->intersectsNode(placeholder);
        if (!intersectsNode.hasException() && intersectsNode.returnValue())
            placeholders.append(placeholder);
    }

    if (placeholders.size() != imageSizes.size()) {
        ASSERT_NOT_REACHED();
        return;
    }

    for (size_t i = 0; i < placeholders.size(); ++i) {
        auto& placeholder = placeholders[i];
        auto imageSize = imageSizes[i];
        double clientWidth = placeholder->clientWidth();
        double heightRespectingAspectRatio = (imageSize.height() * clientWidth) / imageSize.width();
        placeholder->setAttributeWithoutSynchronization(HTMLNames::heightAttr, AtomString::number(heightRespectingAspectRatio));
    }

    document->updateLayout();

    m_droppedImagePlaceholders = WTFMove(placeholders);
    m_droppedImagePlaceholderRange = WTFMove(insertedContentRange);

    frame->selection().clear();
    caretController.setCaretPosition(m_droppedImagePlaceholderRange->startPosition());
}

void DragController::finalizeDroppedImagePlaceholder(HTMLImageElement& placeholder)
{
    ASSERT(placeholder.isDroppedImagePlaceholder());
    placeholder.removeAttribute(HTMLNames::heightAttr);
    placeholder.removeInlineStyleProperty(CSSPropertyBackgroundColor);
}

// Manual drag caret manipulation
void DragController::placeDragCaret(const IntPoint& windowPoint)
{
    mouseMovedIntoDocument(m_page.mainFrame().documentAtPoint(windowPoint));
    if (!m_documentUnderMouse)
        return;
    Frame* frame = m_documentUnderMouse->frame();
    FrameView* frameView = frame->view();
    if (!frameView)
        return;
    IntPoint framePoint = frameView->windowToContents(windowPoint);

    m_page.dragCaretController().setCaretPosition(frame->visiblePositionForPoint(framePoint));
}

bool DragController::shouldUseCachedImageForDragImage(const Image& image) const
{
#if ENABLE(DATA_INTERACTION)
    UNUSED_PARAM(image);
    return true;
#else
    return image.size().height() * image.size().width() <= MaxOriginalImageArea;
#endif
}

#endif // ENABLE(DRAG_SUPPORT)

} // namespace WebCore
