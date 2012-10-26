/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebGraphicsMemoryAllocation_h
#define WebGraphicsMemoryAllocation_h

#include "WebCommon.h"

namespace WebKit {

struct WebGraphicsMemoryAllocation {
    // Deprecated data, to be removed from this structure.
    unsigned gpuResourceSizeInBytes;
    bool suggestHaveBackbuffer;

    enum PriorityCutoff {
        // Allow no allocations.
        PriorityCutoffAllowNothing,
        // Allow only allocations that are visible.
        PriorityCutoffAllowVisibleOnly,
        // Allow only allocations that are few screens away from being visible.
        PriorityCutoffAllowVisibleAndNearby,
        // Allow all allocations.
        PriorityCutoffAllowEverything,
    };

    // Limits when this renderer is visible.
    size_t bytesLimitWhenVisible;
    PriorityCutoff priorityCutoffWhenVisible;

    // Limits when this renderer is not visible.
    size_t bytesLimitWhenNotVisible;
    PriorityCutoff priorityCutoffWhenNotVisible;
    bool haveBackbufferWhenNotVisible;

    // If true, enforce this policy just once, but do not keep
    // it as a permanent policy.
    bool enforceButDoNotKeepAsPolicy;

    WebGraphicsMemoryAllocation()
        : gpuResourceSizeInBytes(0)
        , suggestHaveBackbuffer(false)
        , bytesLimitWhenVisible(0)
        , priorityCutoffWhenVisible(PriorityCutoffAllowNothing)
        , bytesLimitWhenNotVisible(0)
        , priorityCutoffWhenNotVisible(PriorityCutoffAllowNothing)
        , haveBackbufferWhenNotVisible(false)
        , enforceButDoNotKeepAsPolicy(false)
    {
    }

    WebGraphicsMemoryAllocation(unsigned gpuResourceSizeInBytes, bool suggestHaveBackbuffer)
        : gpuResourceSizeInBytes(gpuResourceSizeInBytes)
        , suggestHaveBackbuffer(suggestHaveBackbuffer)
        , bytesLimitWhenVisible(0)
        , priorityCutoffWhenVisible(PriorityCutoffAllowNothing)
        , bytesLimitWhenNotVisible(0)
        , priorityCutoffWhenNotVisible(PriorityCutoffAllowNothing)
        , haveBackbufferWhenNotVisible(false)
        , enforceButDoNotKeepAsPolicy(false)
    {
    }
};

} // namespace WebKit

#endif

