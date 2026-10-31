/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if HAVE(SECURITY_OVERRIDES)

#include <optional>
#include <wtf/CheckedPtr.h>
#include <wtf/CheckedRef.h>
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS WKSecurityOverridesManagerObserver;

namespace WebKit {

class NetworkProcess;

class SecurityOverridesObserver : public CanMakeWeakPtr<SecurityOverridesObserver>, public CanMakeCheckedPtr<SecurityOverridesObserver> {
    WTF_MAKE_TZONE_ALLOCATED(SecurityOverridesObserver);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SecurityOverridesObserver);
    WTF_MAKE_NONCOPYABLE(SecurityOverridesObserver);
public:
    explicit SecurityOverridesObserver(NetworkProcess&);
    ~SecurityOverridesObserver();

    void fetch() WTF_REQUIRES_CAPABILITY(mainRunLoop);
    void setOverridesForTesting(std::optional<HashMap<String, String>>&&) WTF_REQUIRES_CAPABILITY(mainRunLoop);

private:
    void didFetch(uint64_t generation, HashMap<String, String>&&) WTF_REQUIRES_CAPABILITY(mainRunLoop);

    const CheckedRef<NetworkProcess> m_networkProcess;
    Ref<WorkQueue> m_queue;
    RetainPtr<WKSecurityOverridesManagerObserver> m_managerObserver;
    uint64_t m_generation WTF_GUARDED_BY_CAPABILITY(mainRunLoop) { 0 };

#if defined(ENGINEERING_BUILD) && ENGINEERING_BUILD
    std::optional<HashMap<String, String>> m_overridesForTesting WTF_GUARDED_BY_CAPABILITY(mainRunLoop);
#endif // ENGINEERING_BUILD
};

} // namespace WebKit

#endif // HAVE(SECURITY_OVERRIDES)
