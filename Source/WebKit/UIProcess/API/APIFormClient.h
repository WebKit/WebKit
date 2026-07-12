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

#include <wtf/CompletionHandler.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
struct FrameInfoData;
class WebFrameProxy;
class WebPageProxy;
}

namespace API {
class Object;

class FormClient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(FormClient);
public:
    virtual ~FormClient() { }

    // Enforced primary virtual. Subclasses override this and must return a
    // CompletionHandlerCalledToken (proving the handler was called or its
    // call was deferred to a genuine leaf, e.g. stored in a listener proxy or
    // captured into an ObjC block). The base default calls the handler
    // synchronously, which is itself enforced with no deferUnchecked.
    virtual CompletionHandlerCalledToken willSubmitForm(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, WebKit::WebFrameProxy&, WebKit::FrameInfoData&&, WebKit::FrameInfoData&&, const Vector<std::pair<WTF::String, WTF::String>>&, API::Object*, const WTF::URL&, const WTF::String&, CompletionHandler<void(), true>&& completionHandler)
    {
        return completionHandler();
    }

    // Free non-enforced wrapper for callers that still hold a plain
    // CompletionHandler. Constructing the enforced handler from a non-enforced
    // one is free and introduces no deferUnchecked.
    void willSubmitForm(WebKit::WebPageProxy& page, WebKit::WebFrameProxy& frame, WebKit::WebFrameProxy& sourceFrame, WebKit::FrameInfoData&& frameInfoData, WebKit::FrameInfoData&& sourceFrameInfoData, const Vector<std::pair<WTF::String, WTF::String>>& textFieldValues, API::Object* userData, const WTF::URL& requestURL, const WTF::String& method, CompletionHandler<void()>&& completionHandler)
    {
        willSubmitForm(page, frame, sourceFrame, WTF::move(frameInfoData), WTF::move(sourceFrameInfoData), textFieldValues, userData, requestURL, method, CompletionHandler<void(), true>(WTF::move(completionHandler)));
    }
};

} // namespace API
