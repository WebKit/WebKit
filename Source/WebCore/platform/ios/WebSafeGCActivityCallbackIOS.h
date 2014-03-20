/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef WebSafeGCActivityCallbackIOS_h
#define WebSafeGCActivityCallbackIOS_h

#include "WebCoreThread.h"
#include <JavaScriptCore/EdenGCActivityCallback.h>
#include <JavaScriptCore/FullGCActivityCallback.h>

namespace WebCore {

class WebSafeFullGCActivityCallback final : public JSC::FullGCActivityCallback {
public:
    static PassOwnPtr<WebSafeFullGCActivityCallback> create(JSC::Heap* heap)
    {
        return adoptPtr(new WebSafeFullGCActivityCallback(heap));
    }

    virtual ~WebSafeFullGCActivityCallback() override { }

private:
    WebSafeFullGCActivityCallback(JSC::Heap* heap)
        : JSC::FullGCActivityCallback(heap, WebThreadRunLoop())
    {
    }
};

class WebSafeEdenGCActivityCallback final : public JSC::EdenGCActivityCallback {
public:
    static PassOwnPtr<WebSafeEdenGCActivityCallback> create(JSC::Heap* heap)
    {
        return adoptPtr(new WebSafeEdenGCActivityCallback(heap));
    }

    virtual ~WebSafeEdenGCActivityCallback() override { }

private:
    WebSafeEdenGCActivityCallback(JSC::Heap* heap)
        : JSC::EdenGCActivityCallback(heap, WebThreadRunLoop())
    {
    }
};
} // namespace WebCore

#endif // WebSafeGCActivityCallbackIOS_h
