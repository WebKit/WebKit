/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)

#include "Connection.h"
#include "EditingRange.h"
#include "MessageReceiver.h"
#include <WebCore/Range.h>
#include <wtf/Vector.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebCore {
class VisiblePosition;
struct AttributedString;
}

namespace WebKit {

class WebPage;

class TextCheckingControllerProxy : public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    TextCheckingControllerProxy(WebPage&);
    ~TextCheckingControllerProxy();

    static WebCore::AttributedString annotatedSubstringBetweenPositions(const WebCore::VisiblePosition&, const WebCore::VisiblePosition&);

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    struct RangeAndOffset {
        RefPtr<WebCore::Range> range;
        size_t locationInRoot;    
    };
    Optional<RangeAndOffset> rangeAndOffsetRelativeToSelection(int64_t offset, uint64_t length);

    // Message handlers.
    void replaceRelativeToSelection(const WebCore::AttributedString&, int64_t selectionOffset, uint64_t length, uint64_t relativeReplacementLocation, uint64_t relativeReplacementLength);
    void removeAnnotationRelativeToSelection(const String& annotationName, int64_t selectionOffset, uint64_t length);

    WebPage& m_page;
};

} // namespace WebKit

#endif // ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
