/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
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
 *
 */

#pragma once

#if ENABLE(CONTEXT_MENUS)

#include "HitTestResult.h"
#include "Image.h"

namespace WebCore {

class ContextMenuContext {
public:
    enum class Type : uint8_t {
        ContextMenu,
#if ENABLE(SERVICE_CONTROLS)
        ServicesMenu,
#endif // ENABLE(SERVICE_CONTROLS)
#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
        MediaControls,
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    };

    ContextMenuContext();
    ContextMenuContext(Type, const HitTestResult&);

    Type type() const { return m_type; }

    const HitTestResult& hitTestResult() const { return m_hitTestResult; }

    void setSelectedText(const String& selectedText) { m_selectedText = selectedText; }
    const String& selectedText() const { return m_selectedText; }

#if ENABLE(SERVICE_CONTROLS)
    void setControlledImage(Image* controlledImage) { m_controlledImage = controlledImage; }
    Image* controlledImage() const { return m_controlledImage.get(); }
#endif

private:
    Type m_type { Type::ContextMenu };
    HitTestResult m_hitTestResult;
    String m_selectedText;

#if ENABLE(SERVICE_CONTROLS)
    RefPtr<Image> m_controlledImage;
#endif
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::ContextMenuContext::Type> {
    using values = EnumValues<
        WebCore::ContextMenuContext::Type,
        WebCore::ContextMenuContext::Type::ContextMenu
#if ENABLE(SERVICE_CONTROLS)
        , WebCore::ContextMenuContext::Type::ServicesMenu
#endif // ENABLE(SERVICE_CONTROLS)
#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
        , WebCore::ContextMenuContext::Type::MediaControls
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    >;
};

} // namespace WTF

#endif // ENABLE(CONTEXT_MENUS)
