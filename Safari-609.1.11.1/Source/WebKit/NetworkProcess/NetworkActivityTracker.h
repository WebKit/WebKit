/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if USE(APPLE_INTERNAL_SDK) && ((PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 120000) || PLATFORM(WATCHOS))
#define HAVE_NW_ACTIVITY 1
#endif

#if HAVE(NW_ACTIVITY)
#include <nw/private.h>
#include <wtf/RetainPtr.h>
#endif

namespace WebKit {

class NetworkActivityTracker {
public:
    enum class Domain {
        // These are defined to match analogous values used in the Darwin implementation.
        // If they are renumbered, platform-specific code will need to be added to map
        // them to the Darwin-specific values.

        Invalid = 0,
        WebKit = 16,
    };

    enum class Label {
        // These are ours to define, but once defined, they shouldn't change. They can
        // be obsolesced and replaced with other codes, but previously-defined codes
        // should not be renumbered. Previously assigned values should not be re-used.

        Invalid = 0,
        LoadPage = 1,
        LoadResource = 2,
    };

    enum class CompletionCode {
        Undefined,
        None,
        Success,
        Failure,
        Cancel,
    };

    NetworkActivityTracker() = default;
    explicit NetworkActivityTracker(Label, Domain = Domain::WebKit);
    ~NetworkActivityTracker();

    void setParent(NetworkActivityTracker&);
    void start();
    void complete(CompletionCode);

#if HAVE(NW_ACTIVITY)
    nw_activity_t getPlatformObject() { return m_networkActivity.get(); }
#endif

private:
#if HAVE(NW_ACTIVITY)
    Domain m_domain { Domain::Invalid };
    Label m_label { Label::Invalid };
    bool m_isCompleted { false };
    RetainPtr<nw_activity_t> m_networkActivity;
#endif
};

} // namespace WebKit
