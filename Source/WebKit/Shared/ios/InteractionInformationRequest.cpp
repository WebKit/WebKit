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

#import "config.h"
#import "InteractionInformationRequest.h"

#import "ArgumentCodersCF.h"
#import "WebCoreArgumentCoders.h"

namespace WebKit {

#if PLATFORM(IOS_FAMILY)

void InteractionInformationRequest::encode(IPC::Encoder& encoder) const
{
    encoder << point;
    encoder << includeSnapshot;
    encoder << includeLinkIndicator;
    encoder << includeCaretContext;
    encoder << includeHasDoubleClickHandler;
    encoder << linkIndicatorShouldHaveLegacyMargins;
}

bool InteractionInformationRequest::decode(IPC::Decoder& decoder, InteractionInformationRequest& result)
{
    if (!decoder.decode(result.point))
        return false;

    if (!decoder.decode(result.includeSnapshot))
        return false;

    if (!decoder.decode(result.includeLinkIndicator))
        return false;

    if (!decoder.decode(result.includeCaretContext))
        return false;

    if (!decoder.decode(result.includeHasDoubleClickHandler))
        return false;

    if (!decoder.decode(result.linkIndicatorShouldHaveLegacyMargins))
        return false;

    return true;
}

bool InteractionInformationRequest::isValidForRequest(const InteractionInformationRequest& other, int radius) const
{
    if (other.includeSnapshot && !includeSnapshot)
        return false;

    if (other.includeLinkIndicator && !includeLinkIndicator)
        return false;

    if (other.includeCaretContext && !includeCaretContext)
        return false;

    if (other.includeHasDoubleClickHandler && !includeHasDoubleClickHandler)
        return false;

    if (other.linkIndicatorShouldHaveLegacyMargins != linkIndicatorShouldHaveLegacyMargins)
        return false;

    return (other.point - point).diagonalLengthSquared() <= radius * radius;
}
    
bool InteractionInformationRequest::isApproximatelyValidForRequest(const InteractionInformationRequest& other, int radius) const
{
    return isValidForRequest(other, radius);
}

#endif // PLATFORM(IOS_FAMILY)

}
