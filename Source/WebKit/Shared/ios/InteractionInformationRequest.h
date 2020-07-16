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

#if PLATFORM(IOS_FAMILY)

#include "ArgumentCoders.h"
#include "ShareableBitmap.h"
#include <WebCore/IntPoint.h>
#include <WebCore/SelectionRect.h>
#include <WebCore/TextIndicator.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

struct InteractionInformationRequest {
    WebCore::IntPoint point;

    bool includeSnapshot { false };
    bool includeLinkIndicator { false };
    bool includeCaretContext { false };
    bool includeHasDoubleClickHandler { true };

    bool linkIndicatorShouldHaveLegacyMargins { false };

    InteractionInformationRequest() { }
    explicit InteractionInformationRequest(WebCore::IntPoint point)
    {
        this->point = point;
    }

    bool isValidForRequest(const InteractionInformationRequest&, int radius = 0) const;
    bool isApproximatelyValidForRequest(const InteractionInformationRequest&, int radius) const;

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, InteractionInformationRequest&);
};

}

#endif // PLATFORM(IOS_FAMILY)
