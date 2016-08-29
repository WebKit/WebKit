/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef ThreadableLoader_h
#define ThreadableLoader_h

#include "ResourceLoaderOptions.h"
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

    class ResourceError;
    class ResourceRequest;
    class ResourceResponse;
    class ScriptExecutionContext;
    class SecurityOrigin;
    class ThreadableLoaderClient;

    enum PreflightPolicy {
        ConsiderPreflight,
        ForcePreflight,
        PreventPreflight
    };

    enum class ContentSecurityPolicyEnforcement {
        DoNotEnforce,
        EnforceChildSrcDirective,
        EnforceConnectSrcDirective,
        EnforceScriptSrcDirective,
    };

    enum class OpaqueResponseBodyPolicy {
        Receive,
        DoNotReceive
    };

    enum class SameOriginDataURLFlag {
        Set,
        Unset
    };

    struct ThreadableLoaderOptions : ResourceLoaderOptions {
        ThreadableLoaderOptions();
        ThreadableLoaderOptions(const ResourceLoaderOptions&, PreflightPolicy, ContentSecurityPolicyEnforcement, String&& initiator, OpaqueResponseBodyPolicy, SameOriginDataURLFlag);
        ~ThreadableLoaderOptions();

        PreflightPolicy preflightPolicy { ConsiderPreflight };
        ContentSecurityPolicyEnforcement contentSecurityPolicyEnforcement { ContentSecurityPolicyEnforcement::EnforceConnectSrcDirective };
        String initiator; // This cannot be an AtomicString, as isolatedCopy() wouldn't create an object that's safe for passing to another thread.
        OpaqueResponseBodyPolicy opaqueResponse { OpaqueResponseBodyPolicy::Receive };
        SameOriginDataURLFlag sameOriginDataURLFlag { SameOriginDataURLFlag::Unset };
    };

    // Useful for doing loader operations from any thread (not threadsafe,
    // just able to run on threads other than the main thread).
    class ThreadableLoader {
        WTF_MAKE_NONCOPYABLE(ThreadableLoader);
    public:
        static void loadResourceSynchronously(ScriptExecutionContext&, ResourceRequest&&, ThreadableLoaderClient&, const ThreadableLoaderOptions&);
        static RefPtr<ThreadableLoader> create(ScriptExecutionContext&, ThreadableLoaderClient&, ResourceRequest&&, const ThreadableLoaderOptions&);

        virtual void cancel() = 0;
        void ref() { refThreadableLoader(); }
        void deref() { derefThreadableLoader(); }

    protected:
        ThreadableLoader() { }
        virtual ~ThreadableLoader() { }
        virtual void refThreadableLoader() = 0;
        virtual void derefThreadableLoader() = 0;
    };

} // namespace WebCore

#endif // ThreadableLoader_h
