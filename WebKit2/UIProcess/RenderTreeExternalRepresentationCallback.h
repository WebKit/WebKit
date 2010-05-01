/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef RenderTreeExternalRepresentationCallback_h
#define RenderTreeExternalRepresentationCallback_h

#include "WKPagePrivate.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {
    class String;
}

namespace WebKit {

class RenderTreeExternalRepresentationCallback : public RefCounted<RenderTreeExternalRepresentationCallback> {
public:
    static PassRefPtr<RenderTreeExternalRepresentationCallback> create(void* context, WKPageRenderTreeExternalRepresentationFunction callback, WKPageRenderTreeExternalRepresentationDisposeFunction disposeCallback)
    {
        return adoptRef(new RenderTreeExternalRepresentationCallback(context, callback, disposeCallback));
    }
    ~RenderTreeExternalRepresentationCallback();

    uint64_t callbackID() const { return m_callbackID; }

    void performCallbackWithReturnValue(const WebCore::String&);
    void invalidate();

private:
    RenderTreeExternalRepresentationCallback(void*, WKPageRenderTreeExternalRepresentationFunction, WKPageRenderTreeExternalRepresentationDisposeFunction);

    void* m_context;
    WKPageRenderTreeExternalRepresentationFunction m_callback;
    WKPageRenderTreeExternalRepresentationDisposeFunction m_disposeCallback;
    uint64_t m_callbackID;
};

} // namespace WebKit

#endif // RenderTreeExternalRepresentationCallback_h
