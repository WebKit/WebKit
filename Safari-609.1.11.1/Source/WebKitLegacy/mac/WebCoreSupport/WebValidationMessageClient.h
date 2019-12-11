/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#import <WebCore/IntRect.h>
#import <WebCore/ValidationMessageClient.h>

namespace WebCore {
class Element;
}

@class WebView;

class WebValidationMessageClient final : public WebCore::ValidationMessageClient {
public:
    explicit WebValidationMessageClient(WebView*);
    ~WebValidationMessageClient();

    // ValidationMessageClient API.
    void documentDetached(WebCore::Document&) final;
    void showValidationMessage(const WebCore::Element& anchor, const String& message) final;
    void hideValidationMessage(const WebCore::Element& anchor) final;
    void hideAnyValidationMessage() final;
    bool isValidationMessageVisible(const WebCore::Element& anchor) final;
    void updateValidationBubbleStateIfNeeded() final;

private:
    WebView* m_view;
    const WebCore::Element* m_currentAnchor { nullptr };
    WebCore::IntRect m_currentAnchorRect;
};
