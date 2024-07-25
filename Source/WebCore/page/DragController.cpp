/*
 * Copyright (C) 2007-2022 Apple Inc. All rights reserved.
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
#include "SVGElementTypeHelpers.h"

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
#include "ElementAncestorIteratorInlines.h"
#include "EventHandler.h"
#include "EventLoop.h"
#include "File.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameSelection.h"
#include "HTMLAttachmentElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLModelElement.h"
#include "HTMLPlugInElement.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "Image.h"
#include "ImageOrientation.h"
#include "ImageOverlay.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Model.h"
#include "MoveSelectionCommand.h"
#include "MutableStyleProperties.h"
#include "OriginAccessPatterns.h"
#include "Page.h"
#include "Pasteboard.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PluginDocument.h"
#include "PluginViewBase.h"
#include "Position.h"
#include "PromisedAttachmentInfo.h"
#include "Range.h"
#include "RemoteFrame.h"
#include "RemoteUserInputEventData.h"
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
#include "Text.h"
#include "TextEvent.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "VisiblePosition.h"
#include "WebContentReader.h"
#include "markup.h"
#include <wtf/SetForScope.h>
#include <wtf/text/MakeString.h>
#endif

#if ENABLE(DATA_DETECTION)
#include "DataDetection.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "SelectionGeometry.h"
#endif

namespace WebCore {

bool isDraggableLink(const Element& element)
{
    if (RefPtr anchorElement = dynamicDowncast<HTMLAnchorElement>(element)) {
        if (!anchorElement->isLiveLink())
            return false;
#if ENABLE(DATA_DETECTION)
        return !DataDetection::isDataDetectorURL(anchorElement->href());
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
    auto modifiers = PlatformKeyboardEvent::currentStateOfModifierKeys();
    return PlatformMouseEvent(dragData.clientPosition(), dragData.globalPosition(), MouseButton::Left, PlatformEvent::Type::MouseMoved, 0, modifiers, WallTime::now(), ForceAtClick, SyntheticClickType::NoTap);
}

DragController::DragController(Page& page, std::unique_ptr<DragClient>&& client)
    : m_page(page)
    , m_client(WTFMove(client))
{
}

DragController::~DragController() = default;

static RefPtr<DocumentFragment> documentFragmentFromDragData(const DragData& dragData, LocalFrame& frame, const SimpleRange& context, bool allowPlainText, bool& chosePlainText)
{
    chosePlainText = false;

    if (dragData.containsCompatibleContent()) {
        if (RefPtr fragment = frame.editor().webContentFromPasteboard(*Pasteboard::create(dragData), context, allowPlainText, chosePlainText))
            return fragment;

        if (dragData.containsURL(DragData::DoNotConvertFilenames)) {
            String title;
            String url = dragData.asURL(DragData::DoNotConvertFilenames, &title);
            if (!url.isEmpty()) {
                Ref document = context.start.document();
                Ref anchor = HTMLAnchorElement::create(document);
                anchor->setHref(AtomString { url });
                if (title.isEmpty()) {
                    // Try the plain text first because the URL might be normalized or escaped.
                    if (dragData.containsPlainText())
                        title = dragData.asPlainText();
                    if (title.isEmpty())
                        title = url;
                }
                anchor->appendChild(document->createTextNode(WTFMove(title)));
                Ref fragment = document->createDocumentFragment();
                fragment->appendChild(anchor);
                return fragment;
            }
        }
    }
    if (allowPlainText && dragData.containsPlainText()) {
        chosePlainText = true;
        return createFragmentFromText(context, dragData.asPlainText());
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
    m_page->dragCaretController().clear();
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

void DragController::dragExited(LocalFrame& frame, DragData&& dragData)
{
    disallowFileAccessIfNeeded(dragData);
    if (frame.view())
        frame.checkedEventHandler()->cancelDragAndDrop(createMouseEvent(dragData), Pasteboard::create(dragData), dragData.draggingSourceOperationMask(), dragData.containsFiles());
    mouseMovedIntoDocument(nullptr);
    if (RefPtr fileInput = std::exchange(m_fileInputElementUnderMouse, nullptr))
        fileInput->setCanReceiveDroppedFiles(false);
}

inline static bool dragIsHandledByDocument(DragHandlingMethod dragHandlingMethod)
{
    return dragHandlingMethod != DragHandlingMethod::None && dragHandlingMethod != DragHandlingMethod::PageLoad;
}

bool DragController::performDragOperation(DragData&& dragData)
{
    if (!m_droppedImagePlaceholders.isEmpty() && m_droppedImagePlaceholderRange && tryToUpdateDroppedImagePlaceholders(dragData)) {
        m_droppedImagePlaceholders.clear();
        m_droppedImagePlaceholderRange = std::nullopt;
        m_documentUnderMouse = nullptr;
        clearDragCaret();
        return true;
    }

    removeAllDroppedImagePlaceholders();

    SetForScope isPerformingDrop(m_isPerformingDrop, true);
    RefPtr focusedOrMainFrame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!focusedOrMainFrame)
        return false;

    IgnoreSelectionChangeForScope ignoreSelectionChanges { *focusedOrMainFrame };

    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
    if (!localMainFrame)
        return false;

    m_documentUnderMouse = localMainFrame->documentAtPoint(dragData.clientPosition());

    disallowFileAccessIfNeeded(dragData);

    ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy = ShouldOpenExternalURLsPolicy::ShouldNotAllow;
    if (RefPtr document = m_documentUnderMouse)
        shouldOpenExternalURLsPolicy = document->shouldOpenExternalURLsPolicyToPropagate();

    if ((m_dragDestinationActionMask.contains(DragDestinationAction::DHTML)) && dragIsHandledByDocument(m_dragHandlingMethod)) {
        client().willPerformDragDestinationAction(DragDestinationAction::DHTML, dragData);
        bool preventedDefault = false;
        if (localMainFrame->view())
            preventedDefault = localMainFrame->checkedEventHandler()->performDragAndDrop(createMouseEvent(dragData), Pasteboard::create(dragData), dragData.draggingSourceOperationMask(), dragData.containsFiles());
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
    ResourceRequest resourceRequest { urlString };
    resourceRequest.setIsAppInitiated(false);
    FrameLoadRequest frameLoadRequest { *localMainFrame, resourceRequest };
    frameLoadRequest.setShouldOpenExternalURLsPolicy(shouldOpenExternalURLsPolicy);
    frameLoadRequest.setIsRequestFromClientOrUserInput();
    localMainFrame->checkedLoader()->load(WTFMove(frameLoadRequest));
    return true;
}

void DragController::mouseMovedIntoDocument(RefPtr<Document>&& newDocument)
{
    if (m_documentUnderMouse == newDocument)
        return;

    // If we were over another document clear the selection
    if (m_documentUnderMouse)
        clearDragCaret();
    m_documentUnderMouse = WTFMove(newDocument);
}

std::variant<std::optional<DragOperation>, RemoteUserInputEventData> DragController::dragEnteredOrUpdated(LocalFrame& frame, DragData&& dragData)
{
    auto point = frame.protectedView()->windowToContents(dragData.clientPosition());
    auto hitTestResult = HitTestResult(point);
    if (frame.contentRenderer()) {
        constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::DisallowUserAgentShadowContent, HitTestRequest::Type::AllowChildFrameContent };
        hitTestResult = frame.checkedEventHandler()->hitTestResultAtPoint(point, hitType);
    }

    if (RefPtr remoteSubframe = dynamicDowncast<RemoteFrame>(frame.checkedEventHandler()->subframeForTargetNode(hitTestResult.targetNode()))) {
        // FIXME(264611): These mouse coordinates need to be correctly transformed.
        return RemoteUserInputEventData { remoteSubframe->frameID(), dragData.clientPosition() };
    }

    mouseMovedIntoDocument(hitTestResult.innerNode() ? RefPtr { hitTestResult.innerNode()->protectedDocument() } : nullptr);

    m_dragDestinationActionMask = dragData.dragDestinationActionMask();
    if (m_dragDestinationActionMask.isEmpty()) {
        clearDragCaret(); // FIXME: Why not call mouseMovedIntoDocument(nullptr)?
        return std::nullopt;
    }

    disallowFileAccessIfNeeded(dragData);

    std::optional<DragOperation> dragOperation;
    m_dragHandlingMethod = tryDocumentDrag(frame, dragData, m_dragDestinationActionMask, dragOperation);
    if (m_dragHandlingMethod == DragHandlingMethod::None && (m_dragDestinationActionMask.contains(DragDestinationAction::Load))) {
        dragOperation = operationForLoad(dragData);
        if (dragOperation)
            m_dragHandlingMethod = DragHandlingMethod::PageLoad;
    } else if (m_dragHandlingMethod == DragHandlingMethod::SetColor)
        dragOperation = DragOperation::Copy;

    updateSupportedTypeIdentifiersForDragHandlingMethod(m_dragHandlingMethod, dragData);
    return dragOperation;
}

void DragController::disallowFileAccessIfNeeded(DragData& dragData)
{
    RefPtr frame = m_documentUnderMouse ? m_documentUnderMouse->frame() : nullptr;
    if (frame && !frame->eventHandler().canDropCurrentlyDraggedImageAsFile())
        dragData.disallowFileAccess();
}

static RefPtr<HTMLInputElement> asFileInput(Node& node)
{
    RefPtr inputElement = dynamicDowncast<HTMLInputElement>(node);
    if (!inputElement)
        return nullptr;

    // If this is a button inside of the a file input, move up to the file input.
    if (inputElement->isTextButton())
        inputElement = dynamicDowncast<HTMLInputElement>(inputElement->shadowHost());

    return inputElement && inputElement->isFileUpload() ? inputElement : nullptr;
}

#if ENABLE(INPUT_TYPE_COLOR)

static bool isEnabledColorInput(Node& node)
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(node);
    if (!input)
        return false;
    return input->isColorControl() && !input->isDisabledFormControl();
}

static bool isInShadowTreeOfEnabledColorInput(Node& node)
{
    auto* host = node.shadowHost();
    return host && isEnabledColorInput(*host);
}

#endif

// This can return null if an empty document is loaded.
static Element* elementUnderMouse(Document& documentUnderMouse, const IntPoint& p)
{
    RefPtr frame = documentUnderMouse.frame();
    float zoomFactor = frame ? frame->pageZoomFactor() : 1;
    LayoutPoint point(p.x() * zoomFactor, p.y() * zoomFactor);

    HitTestResult result(point);
    documentUnderMouse.hitTest(HitTestRequest(), result);

    RefPtr node = result.innerNode();
    if (!node)
        return nullptr;

    // FIXME: Use parentElementInComposedTree here.
    RefPtr element = dynamicDowncast<Element>(*node);
    if (!element)
        element = node->parentElement();
    auto* host = element->shadowHost();
    return host ? host : element.get();
}

#if !PLATFORM(IOS_FAMILY)

void DragController::updateSupportedTypeIdentifiersForDragHandlingMethod(DragHandlingMethod, const DragData&) const
{
}

#endif

Ref<Page> DragController::protectedPage() const
{
    return m_page.get();
}

DragHandlingMethod DragController::tryDocumentDrag(LocalFrame& frame, const DragData& dragData, OptionSet<DragDestinationAction> destinationActionMask, std::optional<DragOperation>& dragOperation)
{
    if (!m_documentUnderMouse)
        return DragHandlingMethod::None;

    if (m_dragInitiator && !m_documentUnderMouse->protectedSecurityOrigin()->canReceiveDragData(m_dragInitiator->protectedSecurityOrigin()))
        return DragHandlingMethod::None;

    bool isHandlingDrag = false;
    if (destinationActionMask.contains(DragDestinationAction::DHTML)) {
        isHandlingDrag = tryDHTMLDrag(frame, dragData, dragOperation);
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
    RefPtr frameView = m_documentUnderMouse->view();
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
        RefPtr element = elementUnderMouse(*protectedDocumentUnderMouse(), point);
        if (!element)
            return DragHandlingMethod::None;
        
        RefPtr elementAsFileInput = asFileInput(*element);
        if (m_fileInputElementUnderMouse != elementAsFileInput) {
            if (m_fileInputElementUnderMouse)
                m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(false);
            m_fileInputElementUnderMouse = elementAsFileInput;
        }
        
        if (!m_fileInputElementUnderMouse)
            protectedPage()->dragCaretController().setCaretPosition(m_documentUnderMouse->frame()->visiblePositionForPoint(point));
        else
            clearDragCaret();

        RefPtr innerFrame = element->document().frame();
        dragOperation = dragIsMove(innerFrame->checkedSelection().get(), dragData) ? DragOperation::Move : DragOperation::Copy;

        unsigned numberOfFiles = dragData.numberOfFiles();
        if (RefPtr fileInput = m_fileInputElementUnderMouse) {
            if (fileInput->isDisabledFormControl())
                m_numberOfItemsToBeAccepted = 0;
            else if (fileInput->multiple())
                m_numberOfItemsToBeAccepted = numberOfFiles;
            else if (numberOfFiles > 1)
                m_numberOfItemsToBeAccepted = 0;
            else
                m_numberOfItemsToBeAccepted = 1;
            
            if (!m_numberOfItemsToBeAccepted)
                dragOperation = std::nullopt;
            fileInput->setCanReceiveDroppedFiles(m_numberOfItemsToBeAccepted);
        } else {
            // We are not over a file input element. The dragged item(s) will loaded into the view,
            // dropped as text paths on other input elements, or handled by script on the page.
            m_numberOfItemsToBeAccepted = numberOfFiles;
        }

        if (m_fileInputElementUnderMouse)
            return DragHandlingMethod::UploadFile;

        if (m_page->dragCaretController().isContentRichlyEditable())
            return DragHandlingMethod::EditRichText;

        return DragHandlingMethod::EditPlainText;
    }
    
    // We are not over an editable region. Make sure we're clearing any prior drag cursor.
    clearDragCaret();
    if (RefPtr fileInput = std::exchange(m_fileInputElementUnderMouse, nullptr))
        fileInput->setCanReceiveDroppedFiles(false);
    return DragHandlingMethod::None;
}

OptionSet<DragSourceAction> DragController::delegateDragSourceAction(const IntPoint& rootViewPoint)
{
    m_dragSourceAction = client().dragSourceActionMaskForPoint(rootViewPoint);
    return m_dragSourceAction;
}

std::optional<DragOperation> DragController::operationForLoad(const DragData& dragData)
{
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
    if (!localMainFrame)
        return std::nullopt;
    RefPtr document = localMainFrame->documentAtPoint(dragData.clientPosition());

    bool pluginDocumentAcceptsDrags = false;
    if (RefPtr pluginDocument = dynamicDowncast<PluginDocument>(document)) {
        if (RefPtr pluginView = pluginDocument->pluginWidget())
            pluginDocumentAcceptsDrags = pluginView->shouldAllowNavigationFromDrags();
    }

    if (document && (m_didInitiateDrag || (is<PluginDocument>(*document) && !pluginDocumentAcceptsDrags) || document->hasEditableStyle()))
        return std::nullopt;
    return dragOperation(dragData);
}

static bool setSelectionToDragCaret(LocalFrame* frame, VisibleSelection& dragCaret, const IntPoint& point)
{
    Ref protectedFrame { *frame };
    frame->selection().setSelection(dragCaret);
    if (frame->selection().selection().isNone()) {
        dragCaret = frame->visiblePositionForPoint(point);
        frame->checkedSelection()->setSelection(dragCaret);
    }
    return !frame->selection().isNone() && frame->selection().selection().isContentEditable();
}

bool DragController::dispatchTextInputEventFor(LocalFrame* innerFrame, const DragData& dragData)
{
    ASSERT(m_page->dragCaretController().hasCaret());
    String text = m_page->dragCaretController().isContentRichlyEditable() ? emptyString() : dragData.asPlainText();
    auto target = innerFrame->editor().findEventTargetFrom(m_page->dragCaretController().caretPosition());
    // FIXME: What guarantees target is not null?
    Ref event = TextEvent::createForDrop(innerFrame->protectedWindowProxy().ptr(), WTFMove(text));
    target->dispatchEvent(event);
    return !event->defaultPrevented();
}

bool DragController::concludeEditDrag(const DragData& dragData)
{
    RefPtr<HTMLInputElement> fileInput = m_fileInputElementUnderMouse;
    if (RefPtr fileInput = std::exchange(m_fileInputElementUnderMouse, nullptr))
        fileInput->setCanReceiveDroppedFiles(false);

    if (!m_documentUnderMouse)
        return false;

    IntPoint point = m_documentUnderMouse->protectedView()->windowToContents(dragData.clientPosition());
    RefPtr element = elementUnderMouse(*protectedDocumentUnderMouse(), point);
    if (!element)
        return false;
    RefPtr innerFrame = element->document().frame();
    ASSERT(innerFrame);

    if (m_page->dragCaretController().hasCaret() && !dispatchTextInputEventFor(innerFrame.get(), dragData))
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
        Ref style = MutableStyleProperties::create();
        style->setProperty(CSSPropertyColor, serializationForHTML(color));
        if (!innerFrame->checkedEditor()->shouldApplyStyle(style, *innerRange))
            return false;
        client().willPerformDragDestinationAction(DragDestinationAction::Edit, dragData);
        innerFrame->checkedEditor()->applyStyle(style.ptr(), EditAction::SetColor);
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

    if (!m_page->dragController().canProcessDrag(dragData))
        return false;

    VisibleSelection dragCaret = m_page->dragCaretController().caretPosition();
    auto range = dragCaret.toNormalizedRange();
    RefPtr rootEditableElement = innerFrame->selection().selection().rootEditableElement();

    // For range to be null a WebKit client must have done something bad while
    // manually controlling drag behaviour
    if (!range)
        return false;

    ResourceCacheValidationSuppressor validationSuppressor(range->start.document().cachedResourceLoader());
    CheckedRef editor = innerFrame->editor();
    bool isMove = dragIsMove(innerFrame->selection(), dragData);
    if (isMove || dragCaret.isContentRichlyEditable()) {
        bool chosePlainText = false;
        RefPtr fragment = documentFragmentFromDragData(dragData, *innerFrame, *range, true, chosePlainText);
        if (!fragment || !editor->shouldInsertFragment(*fragment, range, EditorInsertAction::Dropped))
            return false;

        client().willPerformDragDestinationAction(DragDestinationAction::Edit, dragData);

        if (editor->client() && editor->client()->performTwoStepDrop(*fragment, *range, isMove))
            return true;

        if (isMove) {
            // NSTextView behavior is to always smart delete on moving a selection,
            // but only to smart insert if the selection granularity is word granularity.
            bool smartDelete = editor->smartInsertDeleteEnabled();
            bool smartInsert = smartDelete && innerFrame->selection().granularity() == TextGranularity::WordGranularity && dragData.canSmartReplace();
            MoveSelectionCommand::create(fragment.releaseNonNull(), dragCaret.base(), smartInsert, smartDelete)->apply();
        } else {
            if (setSelectionToDragCaret(innerFrame.get(), dragCaret, point)) {
                OptionSet<ReplaceSelectionCommand::CommandOption> options { ReplaceSelectionCommand::SelectReplacement, ReplaceSelectionCommand::PreventNesting };
                if (dragData.canSmartReplace())
                    options.add(ReplaceSelectionCommand::SmartReplace);
                if (chosePlainText || dragData.shouldMatchStyleOnDrop())
                    options.add(ReplaceSelectionCommand::MatchStyle);
                ReplaceSelectionCommand::create(protectedDocumentUnderMouse().releaseNonNull(), fragment.releaseNonNull(), options, EditAction::InsertFromDrop)->apply();
            }
        }
    } else {
        String text = dragData.asPlainText();
        if (text.isEmpty() || !editor->shouldInsertText(text, *range, EditorInsertAction::Dropped))
            return false;

        client().willPerformDragDestinationAction(DragDestinationAction::Edit, dragData);
        Ref fragment = createFragmentFromText(*range, text);
        if (editor->client() && editor->client()->performTwoStepDrop(fragment.get(), *range, isMove))
            return true;

        if (setSelectionToDragCaret(innerFrame.get(), dragCaret, point))
            ReplaceSelectionCommand::create(protectedDocumentUnderMouse().releaseNonNull(), WTFMove(fragment), { ReplaceSelectionCommand::SelectReplacement, ReplaceSelectionCommand::MatchStyle, ReplaceSelectionCommand::PreventNesting }, EditAction::InsertFromDrop)->apply();
    }

    if (rootEditableElement) {
        if (RefPtr frame = rootEditableElement->document().frame())
            frame->checkedEventHandler()->updateDragStateAfterEditDragIfNeeded(*rootEditableElement);
    }

    return true;
}

bool DragController::canProcessDrag(const DragData& dragData)
{
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
    if (!localMainFrame)
        return false;
    IntPoint point = localMainFrame->protectedView()->windowToContents(dragData.clientPosition());
    HitTestResult result = HitTestResult(point);
    if (!localMainFrame->contentRenderer())
        return false;

    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AllowChildFrameContent };
    result = localMainFrame->checkedEventHandler()->hitTestResultAtPoint(point, hitType);

    RefPtr dragNode = result.innerNonSharedNode();
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

    if (!dragNode->hasEditableStyle())
        return false;

    if (m_didInitiateDrag && m_documentUnderMouse == m_dragInitiator && result.isSelected())
        return false;

    return true;
}

static std::optional<DragOperation> defaultOperationForDrag(OptionSet<DragOperation> sourceOperationMask)
{
    // This is designed to match IE's operation fallback for the case where
    // the page calls preventDefault() in a drag event but doesn't set dropEffect.
    if (sourceOperationMask.containsAll({ DragOperation::Copy, DragOperation::Link, DragOperation::Generic, DragOperation::Private, DragOperation::Move, DragOperation::Delete }))
        return DragOperation::Copy;
    if (sourceOperationMask.isEmpty())
        return std::nullopt;
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

bool DragController::tryDHTMLDrag(LocalFrame& frame, const DragData& dragData, std::optional<DragOperation>& operation)
{
    ASSERT(m_documentUnderMouse);
    Ref protectedFrame(frame);
    RefPtr view = protectedFrame->view();
    if (!view)
        return false;

    auto sourceOperationMask = dragData.draggingSourceOperationMask();
    auto targetResponse = protectedFrame->checkedEventHandler()->updateDragAndDrop(createMouseEvent(dragData), [&dragData]() {
        return Pasteboard::create(dragData);
    }, sourceOperationMask, dragData.containsFiles());
    if (!targetResponse.accept)
        return false;

    if (!targetResponse.operationMask)
        operation = defaultOperationForDrag(sourceOperationMask);
    else if (!sourceOperationMask.containsAny(targetResponse.operationMask.value())) // The element picked an operation which is not supported by the source.
        operation = std::nullopt;
    else
        operation = defaultOperationForDrag(targetResponse.operationMask.value());

    return true;
}

static bool imageElementIsDraggable(const HTMLImageElement& image, const LocalFrame& sourceFrame)
{
#if ENABLE(MULTI_REPRESENTATION_HEIC)
    if (image.isMultiRepresentationHEIC())
        return false;
#endif

    if (sourceFrame.settings().loadsImagesAutomatically())
        return true;

    CheckedPtr renderImage = dynamicDowncast<RenderImage>(image.renderer());
    if (!renderImage)
        return false;

    CachedResourceHandle cachedImage = renderImage->cachedImage();
    return cachedImage && !cachedImage->errorOccurred() && cachedImage->imageForRenderer(renderImage.get());
}

#if ENABLE(MODEL_ELEMENT)

static bool modelElementIsDraggable(const HTMLModelElement& modelElement)
{
    return modelElement.supportsDragging() && !!modelElement.model();
}

#endif

#if ENABLE(ATTACHMENT_ELEMENT)

static RefPtr<HTMLAttachmentElement> enclosingAttachmentElement(Element& element)
{
    if (RefPtr attachment = dynamicDowncast<HTMLAttachmentElement>(element))
        return attachment;

    return dynamicDowncast<HTMLAttachmentElement>(element.parentOrShadowHostElement());
}

#endif

Element* DragController::draggableElement(const LocalFrame* sourceFrame, Element* startElement, const IntPoint& dragOrigin, DragState& state) const
{
    state.type = sourceFrame->selection().contains(dragOrigin) ? DragSourceAction::Selection : OptionSet<DragSourceAction>({ });
    if (!startElement)
        return nullptr;

#if ENABLE(ATTACHMENT_ELEMENT)
    if (auto attachment = enclosingAttachmentElement(*startElement)) {
        auto& selection = sourceFrame->selection().selection();
        bool isSingleAttachmentSelection = selection.start() == Position(attachment.get(), Position::PositionIsBeforeAnchor) && selection.end() == Position(attachment.get(), Position::PositionIsAfterAnchor);
        auto* renderer = attachment->renderer();
        if (!renderer || renderer->style().userDrag() == UserDrag::None)
            return nullptr;

        auto selectedRange = selection.firstRange();
        if (isSingleAttachmentSelection || !selectedRange || !contains<ComposedTree>(*selectedRange, *attachment)) {
            state.type = DragSourceAction::Attachment;
            return attachment.get();
        }
    }
#endif // ENABLE(ATTACHMENT_ELEMENT)

    RefPtr selectionDragElement = state.type.contains(DragSourceAction::Selection) && m_dragSourceAction.contains(DragSourceAction::Selection) ? startElement : nullptr;
    if (ImageOverlay::isOverlayText(startElement))
        return selectionDragElement.get();

    for (auto* element = startElement; element; element = element->parentOrShadowHostElement()) {
        auto* renderer = element->renderer();
        if (!renderer)
            continue;

        UserDrag dragMode = renderer->style().userDrag();
        if (m_dragSourceAction.contains(DragSourceAction::DHTML) && dragMode == UserDrag::Element) {
            state.type = DragSourceAction::DHTML;
            return element;
        }
        if (dragMode == UserDrag::Auto) {
            if (RefPtr image = dynamicDowncast<HTMLImageElement>(*element); image
                && m_dragSourceAction.contains(DragSourceAction::Image)
                && imageElementIsDraggable(*image, *sourceFrame)) {
                state.type.add(DragSourceAction::Image);
                return element;
            }
            if (m_dragSourceAction.contains(DragSourceAction::Link) && isDraggableLink(*element)) {
                state.type.add(DragSourceAction::Link);
                return element;
            }
#if ENABLE(ATTACHMENT_ELEMENT)
            if (RefPtr attachment = dynamicDowncast<HTMLAttachmentElement>(*element); attachment
                && m_dragSourceAction.contains(DragSourceAction::Attachment)
                && attachment->file()) {
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
#if ENABLE(MODEL_ELEMENT)
            if (RefPtr model = dynamicDowncast<HTMLModelElement>(*element); model
                && m_dragSourceAction.contains(DragSourceAction::Model)
                && modelElementIsDraggable(*model)) {
                state.type.add(DragSourceAction::Model);
                return element;
            }
#endif
        }
    }

    // We either have nothing to drag or we have a selection and we're not over a draggable element.
    return selectionDragElement.get();
}

static CachedImage* getCachedImage(Element& element)
{
    CheckedPtr renderImage = dynamicDowncast<RenderImage>(element.renderer());
    return renderImage ? renderImage->cachedImage() : nullptr;
}

static Image* getImage(Element& element)
{
    CachedResourceHandle cachedImage = getCachedImage(element);
    // Don't use cachedImage->imageForRenderer() here as that may return BitmapImages for cached SVG Images.
    // Users of getImage() want access to the SVGImage, in order to figure out the filename extensions,
    // which would be empty when asking the cached BitmapImages.
    return (cachedImage && !cachedImage->errorOccurred()) ?
        cachedImage->image() : nullptr;
}

static void selectElement(Element& element)
{
    if (RefPtr frame = element.document().frame()) {
        if (auto range = makeRangeSelectingNode(element))
            frame->checkedSelection()->setSelection(*range);
    }
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

static FloatPoint dragImageAnchorPointForSelectionDrag(LocalFrame& frame, const IntPoint& mouseDraggedPoint)
{
    IntRect draggingRect = enclosingIntRect(frame.selection().selectionBounds());

    float x = (mouseDraggedPoint.x() - draggingRect.x()) / (float)draggingRect.width();
    float y = (mouseDraggedPoint.y() - draggingRect.y()) / (float)draggingRect.height();

    return FloatPoint { x, y };
}

static IntPoint dragLocForSelectionDrag(LocalFrame& src)
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

void DragController::prepareForDragStart(LocalFrame& source, OptionSet<DragSourceAction> actionMask, Element& element, DataTransfer& dataTransfer, const IntPoint& dragOrigin) const
{
#if !PLATFORM(WIN)
    Ref protectedSource { source };
    auto hitTestResult = hitTestResultForDragStart(source, element, dragOrigin);
    if (!hitTestResult)
        return;

    auto& pasteboard = dataTransfer.pasteboard();
    CheckedRef editor = source.editor();
    if (actionMask == DragSourceAction::Selection) {
        if (enclosingTextFormControl(source.selection().selection().start()))
            pasteboard.writePlainText(editor->selectedTextForDataTransfer(), Pasteboard::CannotSmartReplace);
        else
            editor->writeSelectionToPasteboard(pasteboard);
        return;
    }

    RefPtr image = getImage(element);
    auto imageURL = hitTestResult->absoluteImageURL();
    if (actionMask.contains(DragSourceAction::Image) && !imageURL.isEmpty() && image && !image->isNull()) {
        editor->writeImageToPasteboard(pasteboard, element, imageURL, { });
        return;
    }

    auto linkURL = hitTestResult->absoluteLinkURL();
    if (actionMask.contains(DragSourceAction::Link) && !linkURL.isEmpty() && source.document()->protectedSecurityOrigin()->canDisplay(linkURL, OriginAccessPatternsForWebProcess::singleton()))
        editor->copyURL(linkURL, hitTestResult->textContent().simplifyWhiteSpace(deprecatedIsSpaceOrNewline), pasteboard);
#else
    // FIXME: Make this work on Windows by implementing Editor::writeSelectionToPasteboard and Editor::writeImageToPasteboard.
    UNUSED_PARAM(source);
    UNUSED_PARAM(actionMask);
    UNUSED_PARAM(element);
    UNUSED_PARAM(dataTransfer);
    UNUSED_PARAM(dragOrigin);
#endif
}

std::optional<HitTestResult> DragController::hitTestResultForDragStart(LocalFrame& source, Element& element, const IntPoint& location) const
{
    if (!source.view() || !source.contentRenderer())
        return std::nullopt;

    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AllowChildFrameContent };
    auto hitTestResult = source.eventHandler().hitTestResultAtPoint(location, hitType);

    bool sourceContainsHitNode = element.containsIncludingShadowDOM(hitTestResult.innerNode());
    if (!sourceContainsHitNode) {
        // The original node being dragged isn't under the drag origin anymore... maybe it was
        // hidden or moved out from under the cursor. Regardless, we don't want to start a drag on
        // something that's not actually under the drag origin.
        return std::nullopt;
    }

    return { hitTestResult };
}

bool DragController::startDrag(LocalFrame& src, const DragState& state, OptionSet<DragOperation> sourceOperationMask, const PlatformMouseEvent& dragEvent, const IntPoint& dragOrigin, HasNonDefaultPasteboardData hasData)
{
    if (!state.source)
        return false;

#if PLATFORM(GTK)
    if (dragEvent.isTouchEvent())
        return false;
#endif

    Ref protectedSrc { src };
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

    Ref dataTransfer = *state.dataTransfer;
    if (state.type == DragSourceAction::DHTML) {
        dragImage = DragImage { dataTransfer->createDragImage(dragImageOffset) };
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
    Ref element = *state.source;

    bool mustUseLegacyDragClient = hasData == HasNonDefaultPasteboardData::Yes || client().useLegacyDragClient();

    RefPtr image = getImage(element);
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

            src.checkedEditor()->willWriteSelectionToPasteboard(*selectionRange);
            auto selection = src.selection().selection();
            bool shouldDragAsPlainText = enclosingTextFormControl(selection.start());
            if (auto range = selection.range(); range && ImageOverlay::isInsideOverlay(*range))
                shouldDragAsPlainText = true;

            if (shouldDragAsPlainText) {
                if (mustUseLegacyDragClient)
                    dataTransfer->pasteboard().writePlainText(src.editor().selectedTextForDataTransfer(), Pasteboard::CannotSmartReplace);
                else {
                    PasteboardWriterData::PlainText plainText;
                    plainText.canSmartCopyOrDelete = false;
                    plainText.text = src.editor().selectedTextForDataTransfer();
                    pasteboardWriterData.setPlainText(WTFMove(plainText));
                }
            } else {
                if (mustUseLegacyDragClient) {
#if !PLATFORM(WIN)
                    src.checkedEditor()->writeSelectionToPasteboard(dataTransfer->pasteboard());
#else
                    // FIXME: Convert Windows to match the other platforms and delete this.
                    dataTransfer->pasteboard().writeSelection(*selectionRange, src.editor().canSmartCopyOrDelete(), src, IncludeImageAltTextForDataTransfer);
#endif
                } else {
#if PLATFORM(COCOA)
                    src.checkedEditor()->writeSelection(pasteboardWriterData);
#endif
                }
            }

            src.checkedEditor()->didWriteSelectionToPasteboard();
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
        dragItem.containsSelection = true;

        beginDrag(WTFMove(dragItem), src, dragOrigin, mouseDraggedPoint, dataTransfer, DragSourceAction::Selection);

        return true;
    }

    if (!src.document()->protectedSecurityOrigin()->canDisplay(linkURL, OriginAccessPatternsForWebProcess::singleton())) {
        src.protectedDocument()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, makeString("Not allowed to drag local resource: "_s, linkURL.stringCenterEllipsizedToLength()));
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
            if (element->isContentRichlyEditable())
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

        String textContentWithSimplifiedWhiteSpace = hitTestResult->textContent().simplifyWhiteSpace(deprecatedIsSpaceOrNewline);

        if (hasData == HasNonDefaultPasteboardData::No) {
            // Simplify whitespace so the title put on the dataTransfer resembles what the user sees
            // on the web page. This includes replacing newlines with spaces.
            if (mustUseLegacyDragClient)
                src.editor().copyURL(linkURL, textContentWithSimplifiedWhiteSpace, dataTransfer->pasteboard());
            else
                pasteboardWriterData.setURLData(src.editor().pasteboardWriterURL(linkURL, textContentWithSimplifiedWhiteSpace));
        } else if (dataTransfer->pasteboard().canWriteTrustworthyWebURLsPboardType()) {
            // Make sure the pasteboard also contains trustworthy link data
            // but don't overwrite more general pasteboard types.
            PasteboardURL pasteboardURL;
            pasteboardURL.url = linkURL;
            pasteboardURL.title = hitTestResult->textContent();
            dataTransfer->pasteboard().writeTrustworthyWebURLsPboardType(pasteboardURL);
        }

        const VisibleSelection& sourceSelection = src.selection().selection();
        if (sourceSelection.isCaret() && sourceSelection.isContentEditable()) {
            // a user can initiate a drag on a link without having any text
            // selected.  In this case, we should expand the selection to
            // the enclosing anchor element
            Position pos = sourceSelection.base();
            if (RefPtr node = enclosingAnchorElement(pos))
                src.checkedSelection()->setSelection(VisibleSelection::selectionFromContentsOfNode(node.get()));
        }

        client().willPerformDragSourceAction(DragSourceAction::Link, dragOrigin, dataTransfer);
        if (!dragImage) {
            TextIndicatorData textIndicator;
            dragImage = DragImage { createDragImageForLink(element, linkURL, textContentWithSimplifiedWhiteSpace, textIndicator, m_page->deviceScaleFactor()) };
            if (dragImage) {
                m_dragOffset = dragOffsetForLinkDragImage(dragImage.get());
                dragLoc = IntPoint(dragOrigin.x() + m_dragOffset.x(), dragOrigin.y() + m_dragOffset.y());
                dragImage = DragImage { platformAdjustDragImageForDeviceScaleFactor(dragImage.get(), m_page->deviceScaleFactor()) };
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
        dragItem.containsSelection = true;

        beginDrag(WTFMove(dragItem), src, dragOrigin, mouseDraggedPoint, dataTransfer, DragSourceAction::Selection);

        return true;
    }

#if ENABLE(ATTACHMENT_ELEMENT)
    if (RefPtr attachment = dynamicDowncast<HTMLAttachmentElement>(element); attachment && m_dragSourceAction.contains(DragSourceAction::Attachment)) {
        src.checkedEditor()->setIgnoreSelectionChanges(true);
        auto previousSelection = src.selection().selection();
        selectElement(element);

        PromisedAttachmentInfo promisedAttachment;
        if (hasData == HasNonDefaultPasteboardData::No) {
            CheckedRef editor = src.editor();
            promisedAttachment = editor->promisedAttachmentInfo(*attachment);
#if PLATFORM(COCOA)
            if (!promisedAttachment && editor->client()) {
                // Otherwise, if no file URL is specified, call out to the injected bundle to populate the pasteboard with data.
                editor->willWriteSelectionToPasteboard(src.selection().selection().toNormalizedRange());
                editor->writeSelectionToPasteboard(dataTransfer->pasteboard());
                editor->didWriteSelectionToPasteboard();
            }
#endif
        }
        
        client().willPerformDragSourceAction(DragSourceAction::Attachment, dragOrigin, dataTransfer);
        
        if (!dragImage) {
            TextIndicatorData textIndicator;
            CheckedPtr attachmentRenderer = dynamicDowncast<RenderAttachment>(attachment->renderer());
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
        if (!element->isContentRichlyEditable())
            src.checkedSelection()->setSelection(previousSelection);
        src.checkedEditor()->setIgnoreSelectionChanges(false);
        return true;
    }
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(state.source); input
        && m_dragSourceAction.contains(DragSourceAction::Color)
        && input->isColorControl()) {
        auto color = input->valueAsColor();

        Path visiblePath;
        dragImage = DragImage { createDragImageForColor(color, input->boundsInRootViewSpace(), input->document().page()->pageScaleFactor(), visiblePath) };
        dragImage.setVisiblePath(visiblePath);
        dataTransfer->pasteboard().write(color);
        dragImageOffset = IntPoint { dragImageSize(dragImage.get()) };
        dragLoc = dragLocForDHTMLDrag(mouseDraggedPoint, dragOrigin, dragImageOffset, false);

        client().willPerformDragSourceAction(DragSourceAction::Color, dragOrigin, dataTransfer);
        doSystemDrag(WTFMove(dragImage), dragLoc, dragOrigin, src, state, { });
        return true;
    }
#endif

#if ENABLE(MODEL_ELEMENT)
    if (RefPtr modelElement = dynamicDowncast<HTMLModelElement>(state.source); modelElement && m_dragSourceAction.contains(DragSourceAction::Model)) {
        dragImage = DragImage { createDragImageForNode(src, *modelElement) };

        PasteboardImage pasteboardImage;
        pasteboardImage.suggestedName = modelElement->currentSrc().lastPathComponent().toString();
        pasteboardImage.resourceMIMEType = modelElement->model()->mimeType();
        pasteboardImage.resourceData = modelElement->model()->data();
        dataTransfer->pasteboard().write(pasteboardImage);

        dragImageOffset = IntPoint { dragImageSize(dragImage.get()) };
        dragLoc = dragLocForDHTMLDrag(mouseDraggedPoint, dragOrigin, dragImageOffset, false);

        client().willPerformDragSourceAction(DragSourceAction::Model, dragOrigin, dataTransfer);
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

void DragController::doImageDrag(Element& element, const IntPoint& dragOrigin, const IntRect& layoutRect, LocalFrame& frame, IntPoint& dragImageOffset, const DragState& state, PromisedAttachmentInfo&& attachmentInfo)
{
    IntPoint mouseDownPoint = dragOrigin;
    DragImage dragImage;
    IntPoint scaledOrigin;

    if (!element.renderer())
        return;

    ImageOrientation orientation = element.renderer()->imageOrientation();

    RefPtr image = getImage(element);
    if (image && !layoutRect.isEmpty() && shouldUseCachedImageForDragImage(*image) && (dragImage = DragImage { createDragImageFromImage(image.get(), orientation) })) {
        dragImage = DragImage { fitDragImageToMaxSize(dragImage.get(), layoutRect.size(), maxDragImageSize()) };
        IntSize fittedSize = dragImageSize(dragImage.get());

        dragImage = DragImage { platformAdjustDragImageForDeviceScaleFactor(dragImage.get(), m_page->deviceScaleFactor()) };
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
        if (CachedResourceHandle cachedImage = getCachedImage(element)) {
            dragImage = DragImage { createDragImageIconForCachedImageFilename(cachedImage->response().suggestedFilename()) };
            if (dragImage) {
                dragImage = DragImage { platformAdjustDragImageForDeviceScaleFactor(dragImage.get(), m_page->deviceScaleFactor()) };
                scaledOrigin = IntPoint(DragIconRightInset - dragImageSize(dragImage.get()).width(), DragIconBottomInset);
            }
        }
    }

    if (!dragImage)
        return;

    dragImageOffset = mouseDownPoint + scaledOrigin;
    doSystemDrag(WTFMove(dragImage), dragImageOffset, dragOrigin, frame, state, WTFMove(attachmentInfo));
}

void DragController::beginDrag(DragItem dragItem, LocalFrame& frame, const IntPoint& mouseDownPoint, const IntPoint& mouseDraggedPoint, DataTransfer& dataTransfer, DragSourceAction dragSourceAction)
{
    ASSERT(!client().useLegacyDragClient());

    m_didInitiateDrag = true;
    m_dragInitiator = frame.document();

    // Protect this frame and view, as a load may occur mid drag and attempt to unload this frame
    Ref protectedLocalMainFrame(m_page->mainFrame());
    RefPtr frameView = frame.view();
    RefPtr mainFrameView = protectedLocalMainFrame->virtualView();

    auto mouseDownPointInRootViewCoordinates = mainFrameView->rootViewToContents(frameView->contentsToRootView(mouseDownPoint));
    auto mouseDraggedPointInRootViewCoordinates = mainFrameView->rootViewToContents(frameView->contentsToRootView(mouseDraggedPoint));

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

void DragController::doSystemDrag(DragImage image, const IntPoint& dragLoc, const IntPoint& eventPos, LocalFrame& frame, const DragState& state, PromisedAttachmentInfo&& promisedAttachmentInfo)
{
    m_didInitiateDrag = true;
    m_dragInitiator = frame.document();
    // Protect this frame and view, as a load may occur mid drag and attempt to unload this frame
    Ref mainFrame = m_page->mainFrame();
    RefPtr frameView = frame.view();
    RefPtr mainFrameView = mainFrame->virtualView();

    DragItem item;
    item.image = WTFMove(image);
    ASSERT(state.type.hasExactlyOneBitSet());
    item.sourceAction = state.type.toSingleValue();
    item.promisedAttachmentInfo = WTFMove(promisedAttachmentInfo);
    item.containsSelection = frame.selection().contains(eventPos);

    auto eventPositionInRootViewCoordinates = frameView->contentsToRootView(eventPos);
    auto dragLocationInRootViewCoordinates = frameView->contentsToRootView(dragLoc);
    item.eventPositionInContentCoordinates = mainFrameView->rootViewToContents(eventPositionInRootViewCoordinates);
    item.dragLocationInContentCoordinates = mainFrameView->rootViewToContents(dragLocationInRootViewCoordinates);
    item.dragLocationInWindowCoordinates = mainFrameView->contentsToWindow(item.dragLocationInContentCoordinates);
    if (RefPtr element = state.source) {
        RefPtr dataTransferImageElement = state.dataTransfer->dragImageElement();
        if (state.type == DragSourceAction::DHTML) {
            // If the drag image has been customized, fall back to positioning the preview relative to the drag event location.
            IntSize dragPreviewSize;
            if (dataTransferImageElement)
                dragPreviewSize = dataTransferImageElement->boundsInRootViewSpace().size();
            else {
                dragPreviewSize = dragImageSize(item.image.get());
                if (RefPtr page = frame.page())
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
            item.url = frame.document()->completeURL(link->getAttribute(HTMLNames::hrefAttr));
        }
    }
    client().startDrag(WTFMove(item), *state.dataTransfer, mainFrame.get());
    // DragClient::startDrag can cause our Page to dispear, deallocating |this|.
    if (!mainFrame->page())
        return;

    cleanupAfterSystemDrag();
}

void DragController::removeAllDroppedImagePlaceholders()
{
    m_droppedImagePlaceholderRange = std::nullopt;
    for (auto& placeholder : std::exchange(m_droppedImagePlaceholders, { })) {
        if (placeholder->isConnected())
            placeholder->remove();
    }
}

bool DragController::tryToUpdateDroppedImagePlaceholders(const DragData& dragData)
{
    ASSERT(!m_droppedImagePlaceholders.isEmpty());
    ASSERT(m_droppedImagePlaceholderRange);

    Ref document = m_droppedImagePlaceholders[0]->document();
    RefPtr frame = document->frame();
    if (!frame)
        return false;

    WebContentReader reader(*frame, *m_droppedImagePlaceholderRange, true);
    auto pasteboard = Pasteboard::create(dragData);
    pasteboard->read(reader);

    RefPtr fragment = reader.protectedFragment();
    if (!fragment)
        return false;

    Vector<Ref<HTMLImageElement>> imageElements;
    for (Ref imageElement : descendantsOfType<HTMLImageElement>(*fragment))
        imageElements.append(WTFMove(imageElement));

    if (imageElements.size() != m_droppedImagePlaceholders.size()) {
        ASSERT_NOT_REACHED();
        return false;
    }

    for (size_t i = 0; i < imageElements.size(); ++i) {
        auto& imageElement = imageElements[i];
        auto& placeholder = m_droppedImagePlaceholders[i];
        placeholder->setAttributeWithoutSynchronization(HTMLNames::srcAttr, imageElement->attributeWithoutSynchronization(HTMLNames::srcAttr));
#if ENABLE(ATTACHMENT_ELEMENT)
        if (RefPtr attachment = imageElement->attachmentElement())
            placeholder->setAttachmentElement(attachment.releaseNonNull());
#endif
    }
    return true;
}

void DragController::insertDroppedImagePlaceholdersAtCaret(const Vector<IntSize>& imageSizes)
{
    auto& caretController = m_page->dragCaretController();
    if (!caretController.isContentRichlyEditable())
        return;

    auto dropCaret = caretController.caretPosition();
    if (dropCaret.isNull())
        return;

    RefPtr document = dropCaret.deepEquivalent().document();
    if (!document)
        return;

    RefPtr frame = document->frame();
    if (!frame)
        return;

    IgnoreSelectionChangeForScope selectionChange { *frame };

    Ref fragment = DocumentFragment::create(*document);
    for (auto& size : imageSizes) {
        ASSERT(!size.isEmpty());
        Ref image = HTMLImageElement::create(*document);
        image->setAttributeWithoutSynchronization(HTMLNames::widthAttr, AtomString::number(size.width()));
        image->setAttributeWithoutSynchronization(HTMLNames::heightAttr, AtomString::number(size.height()));
        image->setInlineStyleProperty(CSSPropertyMaxWidth, 100, CSSUnitType::CSS_PERCENTAGE);
        image->setInlineStyleProperty(CSSPropertyBackgroundColor, serializationForCSS(Color { Color::black.colorWithAlphaByte(13) }));
        image->setIsDroppedImagePlaceholder();
        fragment->appendChild(WTFMove(image));
    }

    frame->checkedSelection()->setSelection(dropCaret);

    Ref command = ReplaceSelectionCommand::create(*document, WTFMove(fragment), { ReplaceSelectionCommand::PreventNesting, ReplaceSelectionCommand::SmartReplace }, EditAction::InsertFromDrop);
    command->apply();

    auto insertedContentRange = command->insertedContentRange();
    if (!insertedContentRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr container = dynamicDowncast<ContainerNode>(commonInclusiveAncestor<ComposedTree>(*insertedContentRange));
    if (!container) {
        ASSERT_NOT_REACHED();
        return;
    }

    Vector<Ref<HTMLImageElement>> placeholders;
    for (auto& placeholder : descendantsOfType<HTMLImageElement>(*container)) {
        if (intersects<ComposedTree>(*insertedContentRange, placeholder))
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

    frame->checkedSelection()->clear();
    caretController.setCaretPosition(makeDeprecatedLegacyPosition(m_droppedImagePlaceholderRange->start));
}

void DragController::finalizeDroppedImagePlaceholder(HTMLImageElement& placeholder, CompletionHandler<void()>&& completion)
{
    placeholder.protectedDocument()->checkedEventLoop()->queueTask(TaskSource::InternalAsyncTask, [completion = WTFMove(completion), placeholder = Ref { placeholder }] () mutable {
        if (placeholder->isDroppedImagePlaceholder()) {
            placeholder->removeAttribute(HTMLNames::heightAttr);
            placeholder->removeInlineStyleProperty(CSSPropertyBackgroundColor);
        }
        completion();
    });
}

// Manual drag caret manipulation
void DragController::placeDragCaret(const IntPoint& windowPoint)
{
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
    if (!localMainFrame)
        return;
    mouseMovedIntoDocument(RefPtr { localMainFrame->documentAtPoint(windowPoint) });
    if (!m_documentUnderMouse)
        return;
    RefPtr frame = m_documentUnderMouse->frame();
    RefPtr frameView = frame->view();
    if (!frameView)
        return;
    IntPoint framePoint = frameView->windowToContents(windowPoint);

    protectedPage()->dragCaretController().setCaretPosition(frame->visiblePositionForPoint(framePoint));
}

bool DragController::shouldUseCachedImageForDragImage(const Image& image) const
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(image);
    return true;
#else
    return image.size().height() * image.size().width() <= MaxOriginalImageArea;
#endif
}

#endif // ENABLE(DRAG_SUPPORT)

} // namespace WebCore
