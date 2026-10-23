/*
 * Copyright (C) 2019 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebPage.h"

#include "DrawingAreaCoordinatedGraphicsGLib.h"
#include "EditorState.h"
#include "InputMethodState.h"
#include "MessageSenderInlines.h"
#include "RenderProcessInfo.h"
#include "UserMessage.h"
#include "WebKitUserMessage.h"
#include "WebKitWebPagePrivate.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include "WebProcessExtensionManager.h"
#include <WebCore/DocumentView.h>
#include <WebCore/Editor.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLTextAreaElement.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformDisplay.h>
#include <WebCore/PointerCharacteristics.h>
#include <WebCore/Range.h>
#include <WebCore/Settings.h>
#include <WebCore/TextIterator.h>
#include <WebCore/UserAgent.h>
#include <WebCore/VisiblePosition.h>
#include <WebCore/VisibleUnits.h>
#include <wtf/text/MakeString.h>

#if ENABLE(2022_GLIB_API)
#include "WebKitWebProcessExtension.h"
#else
#include "WebKitWebExtension.h"
#endif

#if USE(GBM)
#include <WebCore/DRMDeviceManager.h>
#include <WebCore/GBMDevice.h>
#include <WebCore/MemoryMappedGPUBuffer.h>
#include <gbm.h>
#include <xf86drm.h>
#endif

namespace WebKit {
using namespace WebCore;

void WebPage::platformInitialize(const WebPageCreationParameters&)
{
#if USE(ATSPI)
    // Create the accessible object (the plug) that will serve as the
    // entry point to the Web process, and send a message to the UI
    // process to connect the two worlds through the accessibility
    // object there specifically placed for that purpose (the socket).
    if (RefPtr page = corePage()) {
        lazyInitialize(m_accessibilityRootObject, AccessibilityRootAtspi::create(*page));
        m_accessibilityRootObject->registerObject([&](const String& plugID) {
            if (!plugID.isEmpty())
                send(Messages::WebPageProxy::BindAccessibilityTree(plugID));
        });
    }
#endif
}

void WebPage::platformDetach()
{
#if USE(ATSPI)
    if (m_accessibilityRootObject)
        m_accessibilityRootObject->unregisterObject();
#endif
}

void WebPage::sendMessageToWebProcessExtensionWithReply(UserMessage&& message, CompletionHandler<void(UserMessage&&)>&& completionHandler)
{
    auto* extension = WebProcessExtensionManager::singleton().extension();
    if (!extension) {
        completionHandler(UserMessage(message.name, WEBKIT_USER_MESSAGE_UNHANDLED_MESSAGE));
        return;
    }

#if ENABLE(2022_GLIB_API)
    auto* page = webkit_web_process_extension_get_page(extension, m_identifier.toUInt64());
#else
    auto* page = webkit_web_extension_get_page(extension, m_identifier.toUInt64());
#endif
    if (!page) {
        completionHandler(UserMessage(message.name, WEBKIT_USER_MESSAGE_UNHANDLED_MESSAGE));
        return;
    }

    webkitWebPageDidReceiveUserMessage(page, WTF::move(message), WTF::move(completionHandler));
}

void WebPage::sendMessageToWebProcessExtension(UserMessage&& message)
{
    sendMessageToWebProcessExtensionWithReply(WTF::move(message), [](UserMessage&&) { });
}

// The input method only uses the text near the selection, and sending the whole editable content would
// take time linear in its length on every change. The Wayland text-input protocols truncate it to 4000 bytes anyway.
static constexpr unsigned maxSurroundingTextLength = 1000;

static BoundaryPoint surroundingTextStart(const BoundaryPoint& editableStart, const BoundaryPoint& position)
{
    BackwardsCharacterIterator iterator({ editableStart, position });
    if (!iterator.atEnd())
        iterator.advance(maxSurroundingTextLength);
    return iterator.atEnd() ? editableStart : iterator.range().end;
}

static BoundaryPoint surroundingTextEnd(const BoundaryPoint& position, const BoundaryPoint& editableEnd)
{
    return resolveCharacterLocation({ position, editableEnd }, maxSurroundingTextLength);
}

// The limits can split a surrogate pair at the ends of the text. Only the ends cut by the limits are checked, so that an unpaired
// surrogate at the ends of the editable content is kept. Returns the number of code units removed from the start.
static unsigned removeSplitSurrogatePairs(String& text, bool isStartTruncated, bool isEndTruncated)
{
    if (isEndTruncated && !text.isEmpty() && U16_IS_LEAD(text[text.length() - 1]))
        text = text.left(text.length() - 1);
    if (!isStartTruncated || text.isEmpty() || !U16_IS_TRAIL(text[0]))
        return 0;
    text = text.substring(1);
    return 1;
}

void WebPage::getPlatformEditorState(LocalFrame& frame, EditorState& result) const
{
    if (!result.hasPostLayoutAndVisualData() || !frame.view() || frame.view()->needsLayout())
        return;

    auto& postLayoutData = *result.postLayoutData;
    auto& visualData = *result.visualData;
    visualData.caretRectAtStart = frame.selection().absoluteCaretBounds();

    const VisibleSelection& selection = frame.selection().selection();
    if (selection.isNone())
        return;

#if PLATFORM(GTK)
    const Editor& editor = frame.editor();
    if (selection.isRange()) {
        if (editor.selectionHasStyle(CSSPropertyID::FontWeight, "bold"_s) == TriState::True)
            postLayoutData.typingAttributes.add(TypingAttribute::Bold);
        if (editor.selectionHasStyle(CSSPropertyID::FontStyle, "italic"_s) == TriState::True)
            postLayoutData.typingAttributes.add(TypingAttribute::Italics);
        if (editor.selectionHasStyle(CSSPropertyID::WebkitTextDecorationsInEffect, "underline"_s) == TriState::True)
            postLayoutData.typingAttributes.add(TypingAttribute::Underline);
        if (editor.selectionHasStyle(CSSPropertyID::WebkitTextDecorationsInEffect, "line-through"_s) == TriState::True)
            postLayoutData.typingAttributes.add(TypingAttribute::StrikeThrough);
    } else if (selection.isCaret()) {
        if (editor.selectionStartHasStyle(CSSPropertyID::FontWeight, "bold"_s))
            postLayoutData.typingAttributes.add(TypingAttribute::Bold);
        if (editor.selectionStartHasStyle(CSSPropertyID::FontStyle, "italic"_s))
            postLayoutData.typingAttributes.add(TypingAttribute::Italics);
        if (editor.selectionStartHasStyle(CSSPropertyID::WebkitTextDecorationsInEffect, "underline"_s))
            postLayoutData.typingAttributes.add(TypingAttribute::Underline);
        if (editor.selectionStartHasStyle(CSSPropertyID::WebkitTextDecorationsInEffect, "line-through"_s))
            postLayoutData.typingAttributes.add(TypingAttribute::StrikeThrough);
    }
#endif

    if (selection.isContentEditable()) {
        auto selectionStart = selection.visibleStart();
        auto editableRange = makeSimpleRange(startOfEditableContent(selectionStart), endOfEditableContent(selectionStart));
        auto compositionRange = frame.editor().compositionRange();
        if (editableRange && compositionRange && contains<ComposedTree>(*editableRange, *compositionRange)) {
            auto surroundingStart = surroundingTextStart(editableRange->start, compositionRange->start);
            auto surroundingEnd = surroundingTextEnd(compositionRange->end, editableRange->end);
            auto beforeText = plainText({ surroundingStart, compositionRange->start });
            auto text = makeString(beforeText, plainText({ compositionRange->end, surroundingEnd }));
            auto removedLength = removeSplitSurrogatePairs(text, surroundingStart != editableRange->start, surroundingEnd != editableRange->end);
            postLayoutData.surroundingContext = WTF::move(text);
            postLayoutData.surroundingContextCursorPosition = beforeText.length() - removedLength;
            postLayoutData.surroundingContextSelectionPosition = postLayoutData.surroundingContextCursorPosition;
        } else if (auto selectionRange = makeSimpleRange(selectionStart, selection.visibleEnd()); editableRange && selectionRange) {
            auto surroundingStart = surroundingTextStart(editableRange->start, selectionRange->start);
            auto surroundingEnd = surroundingTextEnd(selectionRange->end, editableRange->end);
            auto text = plainText({ surroundingStart, surroundingEnd });
            auto removedLength = removeSplitSurrogatePairs(text, surroundingStart != editableRange->start, surroundingEnd != editableRange->end);
            postLayoutData.surroundingContext = WTF::move(text);
            postLayoutData.surroundingContextCursorPosition = characterCount({ surroundingStart, selectionRange->start }) - removedLength;
            postLayoutData.surroundingContextSelectionPosition = characterCount({ surroundingStart, selectionRange->end }) - removedLength;
        } else
            postLayoutData.surroundingContext = emptyString();
    }
}

static std::optional<InputMethodState> inputMethodStateForElement(Element* element)
{
    if (!element || !element->shouldUseInputMethod())
        return std::nullopt;

    if (auto* textControl = dynamicDowncast<HTMLTextFormControlElement>(*element); textControl && textControl->isReadOnly())
        return std::nullopt;

    InputMethodState state;
    if (is<HTMLInputElement>(*element)) {
        auto& inputElement = downcast<HTMLInputElement>(*element);
        state.setPurposeForInputElement(inputElement);
        // A missing inputmode attribute keeps the purpose derived from the type. A password
        // field keeps its purpose too, because the state has no way to say "secret" separately.
        auto inputMode = inputElement.canonicalInputMode();
        if (inputMode == InputMode::None || (inputMode != InputMode::Unspecified && !inputElement.isPasswordField()))
            state.setPurposeOrHintForInputMode(inputMode);
        // A password revealed by changing the type of the field to text should not be learned either.
        if (inputElement.hasEverBeenPasswordField())
            state.hints.add(InputMethodState::Hint::Private);
#if ENABLE(AUTOCAPITALIZE)
        state.addHintsForAutocapitalizeType(inputElement.autocapitalizeType());
#endif
    } else if (is<HTMLTextAreaElement>(*element) || (element->hasEditableStyle() && is<HTMLElement>(*element))) {
        auto& htmlElement = downcast<HTMLElement>(*element);
        state.setPurposeOrHintForInputMode(htmlElement.canonicalInputMode());
#if ENABLE(AUTOCAPITALIZE)
        state.addHintsForAutocapitalizeType(htmlElement.autocapitalizeType());
#endif
    }

    if (element->isSpellCheckingEnabled())
        state.hints.add(InputMethodState::Hint::Spellcheck);

    return state;
}

void WebPage::setInputMethodState(Element* element)
{
    // Some pages call blur() and then focus() on the focused element in the same task. Hold the reset
    // back until the next run loop iteration, like ElementDidBlur on Cocoa, so that the embedder does not
    // see a focus out and a focus in when the element has the focus again by the end of the task.
    // See elementDidBlur().
    // FIXME: With site isolation, the reset can now reach the UI process after the state sent by the
    // process of a remote frame that got the focus, and the UI process keeps the last state it gets.
    if (!element && m_recentlyBlurredElement)
        return;

    auto state = inputMethodStateForElement(element);

    if (state && !m_userIsInteracting)
        state->hints.add(InputMethodState::Hint::InhibitOnScreenKeyboard);

    if (m_inputMethodState == state)
        return;

    m_inputMethodState = state;
    send(Messages::WebPageProxy::SetInputMethodState(state));
}

void WebPage::focusedElementDidChangeReadOnly(Element& element)
{
    if (m_focusedElement != &element)
        return;

    setInputMethodState(&element);
}

void WebPage::focusedElementDidChangeType(Element& element)
{
    if (m_focusedElement != &element)
        return;

    setInputMethodState(&element);
}

String WebPage::platformUserAgent(const URL& url) const
{
    if (url.isNull() || !m_page->settings().needsSiteSpecificQuirks())
        return String();

    return WebCore::standardUserAgentForURL(url);
}

bool WebPage::hoverSupportedByPrimaryPointingDevice() const
{
    return WebProcess::singleton().primaryPointingDevice() == AvailableInputDevices::Mouse;
}

bool WebPage::hoverSupportedByAnyAvailablePointingDevice() const
{
    return WebProcess::singleton().availableInputDevices().contains(AvailableInputDevices::Mouse);
}

std::optional<PointerCharacteristics> WebPage::pointerCharacteristicsOfPrimaryPointingDevice() const
{
    const auto& primaryPointingDevice = WebProcess::singleton().primaryPointingDevice();
    if (primaryPointingDevice == AvailableInputDevices::Mouse)
        return PointerCharacteristics::Fine;
    if (primaryPointingDevice == AvailableInputDevices::Touchscreen)
        return PointerCharacteristics::Coarse;
    return std::nullopt;
}

OptionSet<PointerCharacteristics> WebPage::pointerCharacteristicsOfAllAvailablePointingDevices() const
{
    OptionSet<PointerCharacteristics> pointerCharacteristics;
    const auto& availableInputs = WebProcess::singleton().availableInputDevices();
    if (availableInputs.contains(AvailableInputDevices::Mouse))
        pointerCharacteristics.add(PointerCharacteristics::Fine);
    if (availableInputs.contains(AvailableInputDevices::Touchscreen))
        pointerCharacteristics.add(PointerCharacteristics::Coarse);
    return pointerCharacteristics;
}

void WebPage::getRenderProcessInfo(CompletionHandler<void(RenderProcessInfo&&)>&& completionHandler)
{
    RenderProcessInfo info;

    auto* display = PlatformDisplay::sharedDisplayIfExists();
    if (!display) {
        completionHandler(WTF::move(info));
        return;
    }

    switch (display->type()) {
#if PLATFORM(GTK) || ENABLE(WPE_PLATFORM)
    case PlatformDisplay::Type::Surfaceless:
        info.platform = "Surfaceless"_s;
        break;
#if USE(GBM)
    case PlatformDisplay::Type::GBM:
        info.platform = "GBM"_s;
        if (auto device = DRMDeviceManager::singleton().mainGBMDevice(DRMDeviceManager::NodeType::Render)) {
            if (drmVersion* version = drmGetVersion(gbm_device_get_fd(device->device()))) {
                info.drmVersion = makeString(unsafeSpan(version->name), " ("_s, unsafeSpan(version->desc), ") "_s, version->version_major, '.', version->version_minor, '.', version->version_patchlevel, ". "_s, unsafeSpan(version->date));
                drmFreeVersion(version);
            }
        }
        break;
#endif
#endif
#if OS(ANDROID)
    case PlatformDisplay::Type::Android:
        info.platform = "Android"_s;
        break;
#endif
#if USE(WPE_RENDERER)
    case PlatformDisplay::Type::WPE:
        info.platform = "WPE"_s;
        break;
#endif
    default:
        break;
    }

    info.msaaSampleCount = display->msaaSampleCount();

#if USE(GBM)
    if (info.platform != "WPE"_s) {
        info.supportedBufferFormats = display->bufferFormats().map([](const auto& format) -> RendererBufferFormat::Format {
            return {
                format.fourcc.value,
                format.modifiers,
            };
        });
    }
    info.dmabufExportStrategy = MemoryMappedGPUBuffer::exportStrategyDescription();
    info.memoryMappedGPUBufferSupported = MemoryMappedGPUBuffer::isSupported();
#endif // USE(GBM)

    static_cast<DrawingAreaCoordinatedGraphics*>(m_drawingArea.get())->fillGLInformation(WTF::move(info), WTF::move(completionHandler));
}

} // namespace WebKit
