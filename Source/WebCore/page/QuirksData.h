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

#include <WebCore/QuirkBehavior.h>
#include <WebCore/QuirkNames.h>
#include <initializer_list>
#include <wtf/Vector.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct QuirksData {
    Vector<QuirkBehavior> behaviors;
    QuirkSiteBitSet sites;

    template<typename BehaviorType>
    void forEachBehavior(NOESCAPE const Invocable<void(const BehaviorType&)> auto& callback) const
    {
        for (auto& behavior : behaviors) {
            if (auto* payload = std::get_if<BehaviorType>(&behavior))
                callback(*payload);
        }
    }

    String scriptsToEvaluateBeforeRunningScript(const URL& scriptURL) const
    {
        StringBuilder builder;
        forEachBehavior<ParameterizedQuirk::EvaluateScriptBeforeRunningScript>([&](auto& behavior) {
            if (!behavior.appliesTo(scriptURL))
                return;
            if (!builder.isEmpty())
                builder.append("\n;\n"_s);
            builder.append(behavior.script);
        });
        return builder.toString();
    }

    inline bool isSite(QuirkSite candidate) const
    {
        return sites.get(static_cast<size_t>(candidate));
    }

    inline void addSite(QuirkSite site)
    {
        sites.set(static_cast<size_t>(site));
    }

    inline bool quirkIsEnabled(SiteSpecificQuirk candidate) const
    {
        return behaviors.containsIf([&](auto& behavior) {
            auto* quirk = std::get_if<SiteSpecificQuirk>(&behavior);
            return quirk && *quirk == candidate;
        });
    }

    inline void enableQuirks()
    {
        // No-op to support macro expansions
    }

    void enableQuirks(std::initializer_list<SiteSpecificQuirk> quirks)
    {
        for (auto quirk : quirks)
            enableQuirk(quirk);
    }

    inline void enableQuirk(SiteSpecificQuirk quirk)
    {
        if (!quirkIsEnabled(quirk))
            behaviors.append(quirk);
    }

    inline void setQuirkState(SiteSpecificQuirk quirk, bool state)
    {
        if (state) {
            enableQuirk(quirk);
            return;
        }

        behaviors.removeAllMatching([&](auto& behavior) {
            auto* candidate = std::get_if<SiteSpecificQuirk>(&behavior);
            return candidate && *candidate == quirk;
        });
    }

    void merge(QuirksData&& other)
    {
        sites.merge(other.sites);

        for (auto& behavior : other.behaviors) {
            if (auto* quirk = std::get_if<SiteSpecificQuirk>(&behavior))
                enableQuirk(*quirk);
            else
                behaviors.append(WTF::move(behavior));
        }
    }
};

} // namespace WebCore
