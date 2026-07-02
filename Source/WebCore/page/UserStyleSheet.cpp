/*
 * Copyright (C) 2024 Igalia S.L. All rights reserved.
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "UserStyleSheet.h"

#include <wtf/HashCountedSet.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringHash.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(UserStyleSheet);

static WTF::URL generateUserStyleUniqueURL()
{
    static uint64_t identifier;
    return { { }, makeString("user-style:"_s, ++identifier) };
}

#if PLATFORM(COCOA)
NO_RETURN_DUE_TO_CRASH NEVER_INLINE static void crashDueToApplicationCreatingUserStyleSheetFromBackgroundThread()
{
    RELEASE_ASSERT_NOT_REACHED("Terminating process due to improper usage of WebKit APIs off the main thread.");
}
#endif

static HashCountedSet<String>& styleStrings()
{
    static NeverDestroyed<HashCountedSet<String>> set;
    return set;
}

static String internedStyleString(const String& string)
{
#if PLATFORM(COCOA)
    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::EnableUserScriptAndUserStyleInterning))
        return string;

    // FIXME: replace this main thread check with a locked HashCountedSet once String becomes fully thread-safe.
    if (!isMainRunLoop())
        crashDueToApplicationCreatingUserStyleSheetFromBackgroundThread();
#endif

    if (string.isEmpty())
        return emptyString();

    auto result = styleStrings().add(string);
    return result.iterator->key;
}

static void removeStyleString(const String& string)
{
#if PLATFORM(COCOA)
    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::EnableUserScriptAndUserStyleInterning))
        return;
#endif

    styleStrings().remove(string);
}

UserStyleSheet::UserStyleSheet(const String& source, const URL& url, Vector<String>&& allowlist, Vector<String>&& blocklist, UserContentInjectedFrames injectedFrames, UserContentMatchParentFrame matchParentFrame, UserStyleLevel level, std::optional<PageIdentifier> pageID)
    : m_source(internedStyleString(source))
    , m_url(url.isEmpty() ? generateUserStyleUniqueURL() : url)
    , m_allowlist(WTF::move(allowlist))
    , m_blocklist(WTF::move(blocklist))
    , m_injectedFrames(injectedFrames)
    , m_matchParentFrame(matchParentFrame)
    , m_level(level)
    , m_pageID(pageID)
{
}

UserStyleSheet::~UserStyleSheet()
{
    removeStyleString(m_source);
}

} // namespace WebCore
