/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

#include <WebCore/QuirkNames.h>
#include <initializer_list>

namespace WebCore {

struct QuirksData {
    QuirkBitSet activeQuirks;
    QuirkSiteBitSet sites;

    inline bool isSite(QuirkSite candidate) const
    {
        return sites.get(static_cast<size_t>(candidate));
    }

    inline void addSite(QuirkSite site)
    {
        sites.set(static_cast<size_t>(site));
    }

    inline bool quirkIsEnabled(SiteSpecificQuirk quirk) const
    {
        return activeQuirks.get(static_cast<size_t>(quirk));
    }

    inline void enableQuirks()
    {
        // No-op to support macro expansions
    }

    constexpr void enableQuirks(std::initializer_list<SiteSpecificQuirk> quirks)
    {
        for (auto quirk : quirks)
            activeQuirks.set(static_cast<size_t>(quirk));
    }

    inline void enableQuirk(SiteSpecificQuirk quirk)
    {
        return activeQuirks.set(static_cast<size_t>(quirk));
    }

    inline void setQuirkState(SiteSpecificQuirk quirk, bool state)
    {
        return activeQuirks.set(static_cast<size_t>(quirk), state);
    }

    constexpr void merge(const QuirksData& other)
    {
        auto& [otherActiveQuirks, otherSites] = other;
        activeQuirks.merge(otherActiveQuirks);
        sites.merge(otherSites);
    }
};

} // namespace WebCore
