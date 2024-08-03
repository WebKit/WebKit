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

#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS NSAttributedString;
OBJC_CLASS NSURL;
OBJC_CLASS SSBServiceLookupResult;

namespace WebKit {

class BrowsingWarning : public RefCounted<BrowsingWarning> {
public:
    struct HTTPSNavigationFailureData { };

    struct SafeBrowsingWarningData {
#if PLATFORM(COCOA)
        RetainPtr<SSBServiceLookupResult> result;
#endif
    };

    using Data = std::variant<SafeBrowsingWarningData, HTTPSNavigationFailureData>;

#if HAVE(SAFE_BROWSING)
    static Ref<BrowsingWarning> create(const URL& url, bool forMainFrameNavigation, Data&& data)
    {
        return adoptRef(*new BrowsingWarning(url, forMainFrameNavigation, WTFMove(data)));
    }
#endif
#if PLATFORM(COCOA)
    static Ref<BrowsingWarning> create(URL&& url, String&& title, String&& warning, RetainPtr<NSAttributedString>&& details, Data&& data)
    {
        return adoptRef(*new BrowsingWarning(WTFMove(url), WTFMove(title), WTFMove(warning), WTFMove(details), WTFMove(data)));
    }
#endif

    const URL& url() const { return m_url; }
    const String& title() const { return m_title; }
    const String& warning() const { return m_warning; }
    bool forMainFrameNavigation() const { return m_forMainFrameNavigation; }
#if PLATFORM(COCOA)
    RetainPtr<NSAttributedString> details() const { return m_details; }
#endif
    const Data& data() const { return m_data; }

    static NSURL *visitUnsafeWebsiteSentinel();
    static NSURL *confirmMalwareSentinel();

private:
#if HAVE(SAFE_BROWSING)
    BrowsingWarning(const URL&, bool, Data&&);
#endif
#if PLATFORM(COCOA)
    BrowsingWarning(URL&&, String&&, String&&, RetainPtr<NSAttributedString>&&, Data&&);
#endif

    URL m_url;
    String m_title;
    String m_warning;
    bool m_forMainFrameNavigation { false };
#if PLATFORM(COCOA)
    RetainPtr<NSAttributedString> m_details;
#endif
    const Data m_data;
};

} // namespace WebKit
