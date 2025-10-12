/*
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

#pragma once

#include <WebCore/FetchRequestDestination.h>
#include <WebCore/FetchRequestMode.h>
#include <WebCore/RunningStatus.h>
#include <WebCore/URLPattern.h>
#include <wtf/FastMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {

struct RouterCondition;
class RouterNotCondition {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(RouterNotCondition);
public:
    RouterNotCondition(RouterCondition&&);

    RouterCondition& value() & { return m_value.get(); }
    RouterCondition&& value() && { return WTFMove(m_value.get()); }

private:
    UniqueRef<RouterCondition> m_value;
};

struct RouterCondition {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(RouterCondition);

    std::optional<URLPattern::Compatible> urlPattern;
    String requestMethod;
    std::optional<FetchRequestMode> requestMode;
    std::optional<FetchRequestDestination> requestDestination;
    std::optional<RunningStatus> runningStatus;

    using Condition = RouterCondition;
    Vector<Condition> orConditions;
    std::optional<RouterNotCondition> notCondition;
};

inline RouterNotCondition::RouterNotCondition(RouterCondition&& value)
    : m_value(makeUniqueRef<RouterCondition>(WTFMove(value)))
{
}

} // namespace WebCore
