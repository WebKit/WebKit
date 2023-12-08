/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "WebPageProxy.h"

#include "EditorState.h"
#include "InputMethodState.h"
#include "PageClientImpl.h"
#include <WebCore/PlatformEvent.h>

#if ENABLE(ACCESSIBILITY)
#include <atk/atk.h>
#endif

#if USE(GBM) && ENABLE(WPE_PLATFORM)
#include "DMABufRendererBufferFormat.h"
#include "MessageSenderInlines.h"
#include "WebPageMessages.h"
#endif

namespace WebKit {

void WebPageProxy::platformInitialize()
{
}

struct wpe_view_backend* WebPageProxy::viewBackend()
{
    return static_cast<PageClientImpl&>(pageClient()).viewBackend();
}

#if ENABLE(WPE_PLATFORM)
WPEView* WebPageProxy::wpeView() const
{
    return static_cast<PageClientImpl&>(pageClient()).wpeView();
}
#endif

void WebPageProxy::bindAccessibilityTree(const String& plugID)
{
#if ENABLE(ACCESSIBILITY)
    auto* accessible = static_cast<PageClientImpl&>(pageClient()).accessible();
    atk_socket_embed(ATK_SOCKET(accessible), const_cast<char*>(plugID.utf8().data()));
    atk_object_notify_state_change(accessible, ATK_STATE_TRANSIENT, FALSE);
#endif
}

void WebPageProxy::didUpdateEditorState(const EditorState&, const EditorState& newEditorState)
{
    if (!newEditorState.shouldIgnoreSelectionChanges)
        pageClient().selectionDidChange();
}

void WebPageProxy::sendMessageToWebViewWithReply(UserMessage&& message, CompletionHandler<void(UserMessage&&)>&& completionHandler)
{
    static_cast<PageClientImpl&>(pageClient()).sendMessageToWebView(WTFMove(message), WTFMove(completionHandler));
}

void WebPageProxy::sendMessageToWebView(UserMessage&& message)
{
    sendMessageToWebViewWithReply(WTFMove(message), [](UserMessage&&) { });
}

void WebPageProxy::setInputMethodState(std::optional<InputMethodState>&& state)
{
    static_cast<PageClientImpl&>(pageClient()).setInputMethodState(WTFMove(state));
}

#if USE(GBM)
Vector<DMABufRendererBufferFormat> WebPageProxy::preferredBufferFormats() const
{
#if ENABLE(WPE_PLATFORM)
    auto* view = wpeView();
    if (!view)
        return { };

    GList* formats = wpe_view_get_preferred_dma_buf_formats(view);
    if (!formats)
        return { };

    Vector<DMABufRendererBufferFormat> dmabufFormats;
    dmabufFormats.reserveInitialCapacity(g_list_length(formats));
    for (GList* i = formats; i; i = g_list_next(i)) {
        auto* format = static_cast<WPEBufferDMABufFormat*>(i->data);
        DMABufRendererBufferFormat dmabufFormat;
        switch (wpe_buffer_dma_buf_format_get_usage(format)) {
        case WPE_BUFFER_DMA_BUF_FORMAT_USAGE_RENDERING:
            dmabufFormat.usage = DMABufRendererBufferFormat::Usage::Rendering;
            break;
        case WPE_BUFFER_DMA_BUF_FORMAT_USAGE_MAPPING:
            dmabufFormat.usage = DMABufRendererBufferFormat::Usage::Mapping;
            break;
        case WPE_BUFFER_DMA_BUF_FORMAT_USAGE_SCANOUT:
            dmabufFormat.usage = DMABufRendererBufferFormat::Usage::Scanout;
            break;
        }
        dmabufFormat.fourcc = wpe_buffer_dma_buf_format_get_fourcc(format);
        auto* modifiers = wpe_buffer_dma_buf_format_get_modifiers(format);
        dmabufFormat.modifiers.reserveInitialCapacity(modifiers->len);
        for (guint i = 0; i < modifiers->len; ++i) {
            guint64* mod = &g_array_index(modifiers, guint64, i);
            dmabufFormat.modifiers.append(*mod);
        }
        dmabufFormats.append(WTFMove(dmabufFormat));
    }
    return dmabufFormats;
#else
    return { };
#endif
}

#if ENABLE(WPE_PLATFORM)
void WebPageProxy::preferredBufferFormatsDidChange()
{
    auto* view = wpeView();
    if (!view)
        return;

    send(Messages::WebPage::PreferredBufferFormatsDidChange(preferredBufferFormats()));
}
#endif
#endif

OptionSet<WebCore::PlatformEvent::Modifier> WebPageProxy::currentStateOfModifierKeys()
{
#if ENABLE(WPE_PLATFORM)
    auto* view = wpeView();
    if (!view)
        return { };

    auto* keymap = wpe_display_get_keymap(wpe_view_get_display(view), nullptr);
    if (!keymap)
        return { };

    OptionSet<WebCore::PlatformEvent::Modifier> modifiers;
    auto wpeModifiers = wpe_keymap_get_modifiers(keymap);
    if (wpeModifiers & WPE_MODIFIER_KEYBOARD_CONTROL)
        modifiers.add(WebCore::PlatformEvent::Modifier::ControlKey);
    if (wpeModifiers & WPE_MODIFIER_KEYBOARD_SHIFT)
        modifiers.add(WebCore::PlatformEvent::Modifier::ShiftKey);
    if (wpeModifiers & WPE_MODIFIER_KEYBOARD_ALT)
        modifiers.add(WebCore::PlatformEvent::Modifier::AltKey);
    if (wpeModifiers & WPE_MODIFIER_KEYBOARD_META)
        modifiers.add(WebCore::PlatformEvent::Modifier::MetaKey);
    if (wpeModifiers & WPE_MODIFIER_KEYBOARD_CAPS_LOCK)
        modifiers.add(WebCore::PlatformEvent::Modifier::CapsLockKey);
    return modifiers;
#else
    return { };
#endif
}

} // namespace WebKit
