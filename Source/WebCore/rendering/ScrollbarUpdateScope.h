/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/ScrollTypes.h>

#include <wtf/CheckedPtr.h>
#include <wtf/EnumSet.h>

namespace WebCore {

class RenderLayerScrollableArea;

enum class HasHorizontalOverflow : bool { No, Yes };
enum class HasVerticalOverflow : bool { No, Yes };

// Used to inform layout of any changes to scrollbars. We capture any state associated
// with these changes to perform any remaining work after layout has responded since it
// may need to layout the renderer again.
class ScrollbarUpdateScope {
public:
    ScrollbarUpdateScope(RenderLayerScrollableArea&, ScrollPosition originalScrollPosition, EnumSet<ScrollbarOrientation> autoScrollbarChanges, HasHorizontalOverflow, HasVerticalOverflow);
    ~ScrollbarUpdateScope();

    ScrollbarUpdateScope(ScrollbarUpdateScope&&) = default;
    ScrollbarUpdateScope(const ScrollbarUpdateScope&) = delete;
    ScrollbarUpdateScope& operator=(const ScrollbarUpdateScope&) = delete;

    const EnumSet<ScrollbarOrientation>& autoScrollbarChanges() const { return m_autoScrollbarChanges; }

private:
    const CheckedRef<RenderLayerScrollableArea> m_renderLayerScrollableArea;
    const ScrollPosition m_originalScrollPosition;
    const EnumSet<ScrollbarOrientation> m_autoScrollbarChanges;
    HasHorizontalOverflow m_hasHorizontalOverflow;
    HasVerticalOverflow m_hasVerticalOverflow;
};

}
