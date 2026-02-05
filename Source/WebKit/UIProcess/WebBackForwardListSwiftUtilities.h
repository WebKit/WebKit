/*
 * Copyright (C) 2025-2026 Apple Inc. All rights reserved.
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

#include "Logging.h"
#include "WebBackForwardListItem.h"
#include "WebBackForwardListMessages.h"
#include "WebProcessProxy.h"
#include <cstdint>
#include <wtf/Function.h>
#include <wtf/Markable.h>
#include <wtf/RefCountable.h>
#include <wtf/RefCounted.h>
#include <wtf/SwiftBridging.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

#if ENABLE(BACK_FORWARD_LIST_SWIFT)

// Workaround for rdar://162358154
using SpanConstChar = std::span<const char>;

// These can't be inline due to rdar://162531519
void doLog(const char* WTF_NONNULL msg); // rdar://168139823
void doLoadingReleaseLog(const char* WTF_NONNULL msg); // rdar://168139823
void messageCheckFailed(Ref<WebKit::WebProcessProxy>); // rdar://168139740

// Workaround for rdar://162357139
template<typename T>
inline bool contentsMatch(const T& lhs, const T& rhs)
{
    return lhs == rhs;
}

// Workaround for rdar://162193891
WebCore::BackForwardFrameItemIdentifier generateBackForwardFrameItemIdentifier();
WebCore::BackForwardItemIdentifier generateBackForwardItemIdentifier();

// Workaround for rdar://129159672
inline void setOptionalUInt32Value(std::optional<uint32_t>& optional, uint32_t value)
{
    optional = value;
}

using WebBackForwardListItemFilter = WTF::RefCountable<WTF::Function<bool (WebKit::WebBackForwardListItem&)>>;

#endif // ENABLE(BACK_FORWARD_LIST_SWIFT)
