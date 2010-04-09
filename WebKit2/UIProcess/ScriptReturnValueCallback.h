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

#ifndef ScriptReturnValueCallback_h
#define ScriptReturnValueCallback_h

#include "WKBase.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {
    class StringImpl;
}

namespace WebKit {

class ScriptReturnValueCallback : public RefCounted<ScriptReturnValueCallback> {
public:
    typedef void (*ScriptReturnValueCallbackFunction)(void*, WKStringRef);
    typedef void (*ScriptReturnValueCallbackDisposeFunction)(void*);

    static PassRefPtr<ScriptReturnValueCallback> create(void* context, ScriptReturnValueCallbackFunction callback, ScriptReturnValueCallbackDisposeFunction disposeCallback)
    {
        return adoptRef(new ScriptReturnValueCallback(context, callback, disposeCallback));
    }
    ~ScriptReturnValueCallback();

    uint64_t callbackID() const { return m_callbackID; }

    void performCallbackWithReturnValue(WebCore::StringImpl*);

private:
    ScriptReturnValueCallback(void*, ScriptReturnValueCallbackFunction, ScriptReturnValueCallbackDisposeFunction);

    void* m_context;
    ScriptReturnValueCallbackFunction m_callback;
    ScriptReturnValueCallbackDisposeFunction m_disposeCallback;
    uint64_t m_callbackID;
};

} // namespace WebKit

#endif // ScriptReturnValueCallback_h
