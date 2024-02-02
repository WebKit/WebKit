/*
 * Copyright (C) 2023 Sony Interactive Entertainment Inc.
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

#pragma once

#include "APIObject.h"
#include "WebAccessibilityObjectData.h"
#include <wtf/RefPtr.h>

namespace WebKit {

class WebPageProxy;
class WebFrameProxy;

class WebAccessibilityObject : public API::ObjectImpl<API::Object::Type::AccessibilityObject> {
public:
    static RefPtr<WebAccessibilityObject> create(const WebAccessibilityObjectData&, WebFrameProxy*);

    WebCore::AccessibilityRole role() const { return m_data.role(); }
    String title() const { return m_data.title(); }
    String description() const { return m_data.description(); }
    String helpText() const { return m_data.helpText(); }
    URL url() const { return m_data.url(); }
    WebCore::IntRect rect() const { return m_data.rect(); }
    WebCore::AccessibilityButtonState buttonState() const { return m_data.buttonState(); }
    String value() const { return m_data.value(); }
    bool isFocused() const { return m_data.isFocused(); }
    bool isDisabled() const { return m_data.isDisabled(); }
    bool isSelected() const { return m_data.isSelected(); }
    bool isVisited() const { return m_data.isVisited(); }
    bool isLinked() const { return m_data.isLinked(); }

    WebPageProxy* page();
    WebFrameProxy* frame() { return m_frame; };

private:
    WebAccessibilityObject(const WebAccessibilityObjectData&, WebFrameProxy*);

    WebFrameProxy* m_frame;
    WebAccessibilityObjectData m_data;
};

} // namespace WebKit
