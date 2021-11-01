/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#if USE(GRAPHICS_LAYER_WC)

#include "WCSceneContext.h"
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>

namespace WebKit {

// Creating a WCSceneContext for a window fails if the window already
// has one. While navigating to a new page with a new WebProcess, the
// provisional WebKit::WebPage in the new WebProcess has the same
// window handle. WCSceneContext should be shared among all
// RemoteWCLayerTreeHosts that render to the same native window.
class WCSharedSceneContextHolder {
    WTF_MAKE_NONCOPYABLE(WCSharedSceneContextHolder);
public:
    WCSharedSceneContextHolder() = default;

    struct Holder : RefCounted<Holder> {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        int64_t nativeWindow;
        // The following member is accessed in the OpenGL thread
        std::optional<WCSceneContext> context;
    };

    Ref<Holder> ensureHolderForWindow(int64_t nativeWindow)
    {
        ASSERT(RunLoop::isMain());
        auto result = m_hash.add(nativeWindow, nullptr);
        if (!result.isNewEntry) 
            return *result.iterator->value;
        auto holder = adoptRef(*new Holder);
        holder->nativeWindow = nativeWindow;
        result.iterator->value = holder.ptr();
        return holder;
    }

    RefPtr<Holder> removeHolder(Ref<Holder>&& holder)
    {
        ASSERT(RunLoop::isMain());
        if (!holder->hasOneRef()) 
            return nullptr;
        m_hash.remove(holder->nativeWindow);
        return holder;
    }

private:
    HashMap<int64_t, Holder*> m_hash;
};

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
