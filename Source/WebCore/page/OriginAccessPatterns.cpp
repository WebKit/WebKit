/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "OriginAccessPatterns.h"

#include "RuntimeApplicationChecks.h"
#include "UserContentURLPattern.h"
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

// FIXME: Instead of having a singleton, this should be owned by Page.
OriginAccessPatternsForWebProcess& OriginAccessPatternsForWebProcess::singleton()
{
    // FIXME: We ought to be able to assert that isInWebProcess() is true, but
    // WebKitLegacy doesn't have a web content process and API::ContentRuleListStore
    // uses a CSSParserContext in the UI process. Use EmptyOriginAccessPatterns for that.
    ASSERT(!isInNetworkProcess());
    ASSERT(!isInGPUProcess());

    static NeverDestroyed<OriginAccessPatternsForWebProcess> instance;
    return instance.get();
}

static Lock originAccessPatternLock;
static Vector<UserContentURLPattern>& originAccessPatterns() WTF_REQUIRES_LOCK(originAccessPatternLock)
{
    ASSERT(originAccessPatternLock.isHeld());
    static NeverDestroyed<Vector<UserContentURLPattern>> originAccessPatterns;
    return originAccessPatterns;
}

void OriginAccessPatternsForWebProcess::allowAccessTo(const UserContentURLPattern& pattern)
{
    Locker locker { originAccessPatternLock };
    originAccessPatterns().append(pattern);
}

bool OriginAccessPatternsForWebProcess::anyPatternMatches(const URL& url) const
{
    Locker locker { originAccessPatternLock };
    for (const auto& pattern : originAccessPatterns()) {
        if (pattern.matches(url))
            return true;
    }
    return false;
}

const EmptyOriginAccessPatterns& EmptyOriginAccessPatterns::singleton()
{
    ASSERT(!isInWebProcess());
    static NeverDestroyed<EmptyOriginAccessPatterns> instance;
    return instance.get();
}

bool EmptyOriginAccessPatterns::anyPatternMatches(const URL&) const
{
    return false;
}

const OriginAccessPatterns& originAccessPatternsForWebProcessOrEmpty()
{
    if (isInWebProcess())
        return OriginAccessPatternsForWebProcess::singleton();
    return EmptyOriginAccessPatterns::singleton();
}

}
