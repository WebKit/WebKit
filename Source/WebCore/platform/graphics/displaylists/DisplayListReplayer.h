/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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

#include "GraphicsContext.h"
#include "InMemoryDisplayList.h"
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class FloatRect;

namespace DisplayList {

class ResourceHeap;

enum class StopReplayReason : uint8_t {
    ReplayedAllItems,
    MissingCachedResource,
    InvalidItemOrExtent,
    OutOfMemory
};

struct ReplayResult {
    std::unique_ptr<InMemoryDisplayList> trackedDisplayList;
    size_t numberOfBytesRead { 0 };
    std::optional<RenderingResourceIdentifier> missingCachedResourceIdentifier;
    StopReplayReason reasonForStopping { StopReplayReason::ReplayedAllItems };
};

class Replayer {
    WTF_MAKE_NONCOPYABLE(Replayer);
public:
    WEBCORE_EXPORT Replayer(GraphicsContext&, const DisplayList&, const ResourceHeap* = nullptr);
    ~Replayer() = default;

    WEBCORE_EXPORT ReplayResult replay(const FloatRect& initialClip = { }, bool trackReplayList = false);

private:
    struct ApplyItemResult {
        std::optional<StopReplayReason> stopReason;
        std::optional<RenderingResourceIdentifier> resourceIdentifier;
    };
    ApplyItemResult applyItem(ItemHandle);

    GraphicsContext& m_context;
    const DisplayList& m_displayList;
    const ResourceHeap& m_resourceHeap;
};

}
}

