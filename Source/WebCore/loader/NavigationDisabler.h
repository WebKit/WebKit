/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "LocalFrame.h"

namespace WebCore {

class NavigationDisabler {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Loader);
public:
    NavigationDisabler(LocalFrame* frame)
        : m_frame(frame)
    {
        if (frame) {
            if (auto* localFrame = dynamicDowncast<LocalFrame>(frame->mainFrame()))
                ++localFrame->m_navigationDisableCount;
        } else // Disable all navigations when destructing a frame-less document.
            ++s_globalNavigationDisableCount;
    }

    ~NavigationDisabler()
    {
        if (m_frame) {
            if (auto* mainFrame = dynamicDowncast<LocalFrame>(m_frame->mainFrame())) {
                ASSERT(mainFrame->m_navigationDisableCount);
                --mainFrame->m_navigationDisableCount;
            }
        } else {
            ASSERT(s_globalNavigationDisableCount);
            --s_globalNavigationDisableCount;
        }
    }

    static bool isNavigationAllowed(Frame& frame)
    {
        if (auto* localFrame = dynamicDowncast<LocalFrame>(frame.mainFrame()))
            return !localFrame->m_navigationDisableCount && !s_globalNavigationDisableCount;
        return true;
    }

private:
    RefPtr<LocalFrame> m_frame;

    static unsigned s_globalNavigationDisableCount;
};

}
