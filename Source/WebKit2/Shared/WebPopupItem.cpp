/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#include "WebPopupItem.h"

#include "ArgumentCoders.h"
#include "Arguments.h"

using namespace WebCore;

namespace WebKit {

WebPopupItem::WebPopupItem()
    : m_type(Item)
    , m_textDirection(LTR)
    , m_hasTextDirectionOverride(false)
    , m_isEnabled(true)
{
}

WebPopupItem::WebPopupItem(Type type)
    : m_type(type)
    , m_textDirection(LTR)
    , m_hasTextDirectionOverride(false)
    , m_isEnabled(true)
    , m_isLabel(false)
{
}

WebPopupItem::WebPopupItem(Type type, const String& text, TextDirection textDirection, bool hasTextDirectionOverride, const String& toolTip, const String& accessibilityText, bool isEnabled, bool isLabel)
    : m_type(type)
    , m_text(text)
    , m_textDirection(textDirection)
    , m_hasTextDirectionOverride(hasTextDirectionOverride)
    , m_toolTip(toolTip)
    , m_accessibilityText(accessibilityText)
    , m_isEnabled(isEnabled)
    , m_isLabel(isLabel)
{
}

void WebPopupItem::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encode(CoreIPC::In(static_cast<uint32_t>(m_type), m_text, static_cast<uint64_t>(m_textDirection), m_hasTextDirectionOverride, m_toolTip, m_accessibilityText, m_isEnabled, m_isLabel));
}

bool WebPopupItem::decode(CoreIPC::ArgumentDecoder* decoder, WebPopupItem& item)
{
    uint32_t type;
    String text;
    uint64_t textDirection;
    bool hasTextDirectionOverride;
    String toolTip;
    String accessibilityText;
    bool isEnabled;
    bool isLabel;
    if (!decoder->decode(CoreIPC::Out(type, text, textDirection, hasTextDirectionOverride, toolTip, accessibilityText, isEnabled, isLabel)))
        return false;

    item = WebPopupItem(static_cast<Type>(type), text, static_cast<TextDirection>(textDirection), hasTextDirectionOverride, toolTip, accessibilityText, isEnabled, isLabel);
    return true;
}

} // namespace WebKit
