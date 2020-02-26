/*
 * Copyright (C) 2012, 2014, 2016 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Assertions.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class UserAgentQuirks {
public:
    enum UserAgentQuirk {
        NeedsChromeBrowser,
        NeedsFirefoxBrowser,
        NeedsMacintoshPlatform,
        NeedsLinuxDesktopPlatform,

        NumUserAgentQuirks
    };

    UserAgentQuirks()
        : m_quirks(0)
    {
        COMPILE_ASSERT(sizeof(m_quirks) * 8 >= NumUserAgentQuirks, not_enough_room_for_quirks);
    }

    void add(UserAgentQuirk quirk)
    {
        ASSERT(quirk >= 0);
        ASSERT_WITH_SECURITY_IMPLICATION(quirk < NumUserAgentQuirks);

        m_quirks |= (1 << quirk);
    }

    bool contains(UserAgentQuirk quirk) const
    {
        return m_quirks & (1 << quirk);
    }

    bool isEmpty() const { return !m_quirks; }

    static UserAgentQuirks quirksForURL(const URL&);

    static String stringForQuirk(UserAgentQuirk);

private:
    uint32_t m_quirks;
};

}
