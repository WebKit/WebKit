/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebSurroundingText_h
#define WebSurroundingText_h

#include "WebNode.h"
#include "WebRange.h"
#include "platform/WebPrivateOwnPtr.h"
#include "platform/WebString.h"

namespace WebCore {
class SurroundingText;
}

namespace WebKit {

class WebHitTestResult;

class WebSurroundingText {
public:
    WebSurroundingText() { }
    ~WebSurroundingText() { reset(); }

    WEBKIT_EXPORT bool isNull() const;
    WEBKIT_EXPORT void reset();

    // Initializes the object go get the surrounding text centered in the position described by the hit test.
    // The maximum length of the contents retrieved is defined by maxLength.
    WEBKIT_EXPORT void initialize(const WebHitTestResult&, size_t maxLength);

    // Initializes the object go get the surrounding text centered in the selected offset of the given node.
    // The maximum length of the contents retrieved is defined by maxLength.
    WEBKIT_EXPORT void initialize(WebNode textNode, size_t offset, size_t maxLength);

    // Surrounding text content retrieved.
    WEBKIT_EXPORT WebString textContent() const;

    // Offset in the text content of the initial hit position (or provided offset in the node).
    WEBKIT_EXPORT size_t hitOffsetInTextContent() const;

    // Convert start/end positions in the content text string into a WebKit text range.
    WEBKIT_EXPORT WebRange rangeFromContentOffsets(size_t startOffsetInContent, size_t endOffsetInContent);

protected:
    WebPrivateOwnPtr<WebCore::SurroundingText> m_private;
};

} // namespace WebKit

#endif
