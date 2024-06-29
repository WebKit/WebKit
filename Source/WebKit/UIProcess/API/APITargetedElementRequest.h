/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include <WebCore/ElementTargetingTypes.h>

namespace WebKit {
class WebPageProxy;
}

namespace API {

class TargetedElementRequest final : public ObjectImpl<Object::Type::TargetedElementRequest> {
public:

    bool shouldIgnorePointerEventsNone() const { return m_request.shouldIgnorePointerEventsNone; }
    void setShouldIgnorePointerEventsNone(bool value) { m_request.shouldIgnorePointerEventsNone = value; }

    bool canIncludeNearbyElements() const { return m_request.canIncludeNearbyElements; }
    void setCanIncludeNearbyElements(bool value) { m_request.canIncludeNearbyElements = value; }

    WebCore::TargetedElementRequest makeRequest(const WebKit::WebPageProxy&) const;

    WebCore::FloatPoint point() const;
    void setPoint(WebCore::FloatPoint);

    void setSearchText(WTF::String&&);
    void setSelectors(WebCore::TargetedElementSelectors&&);

private:
    WebCore::TargetedElementRequest m_request;
};


} // namespace API
