/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8RecursionScope_h
#define V8RecursionScope_h

#include "ScriptExecutionContext.h"
#include "V8Binding.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

// C++ calls into script contexts which are "owned" by WebKit (created in a
// process where WebKit.cpp initializes v8) must declare their type:
//
//   1. Calls into page/author script from a frame
//   2. Calls into page/author script from a worker
//   3. Calls into internal script (typically setup/teardown work)
//
// Debug-time checking of this is enforced via this class.
//
// Calls of type (1) should generally go through V8Proxy, as inspector
// instrumentation is needed. Calls of type (2) should always stack-allocate a
// V8RecursionScope in the same block as the call into script. Calls of type (3)
// should stack allocate a V8RecursionScope::MicrotaskSuppression -- this
// skips work that is spec'd to happen at the end of the outer-most script stack
// frame of calls into page script:
//
// http://www.whatwg.org/specs/web-apps/current-work/#perform-a-microtask-checkpoint
class V8RecursionScope {
    WTF_MAKE_NONCOPYABLE(V8RecursionScope);
public:
    explicit V8RecursionScope(ScriptExecutionContext* context)
        : m_isDocumentContext(context && context->isDocument())
    {
        V8BindingPerIsolateData::current()->incrementRecursionLevel();
    }

    ~V8RecursionScope()
    {
        if (!V8BindingPerIsolateData::current()->decrementRecursionLevel())
            didLeaveScriptContext();
    }

    static int recursionLevel()
    {
        return V8BindingPerIsolateData::current()->recursionLevel();
    }

#ifndef NDEBUG
    static bool properlyUsed()
    {
        return recursionLevel() > 0 || V8BindingPerIsolateData::current()->internalScriptRecursionLevel() > 0;
    }
#endif

    class MicrotaskSuppression {
    public:
        MicrotaskSuppression()
        {
#ifndef NDEBUG
            V8BindingPerIsolateData::current()->incrementInternalScriptRecursionLevel();
#endif
        }

        ~MicrotaskSuppression()
        {
#ifndef NDEBUG
            V8BindingPerIsolateData::current()->decrementInternalScriptRecursionLevel();
#endif
        }
    };

private:
    void didLeaveScriptContext();

    bool m_isDocumentContext;
};

} // namespace WebCore

#endif // V8RecursionScope_h
