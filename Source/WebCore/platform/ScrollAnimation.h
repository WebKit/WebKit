/*
 * Copyright (C) 2016 Igalia S.L.
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

#ifndef ScrollAnimation_h
#define ScrollAnimation_h

#include "ScrollTypes.h"

namespace WebCore {

class FloatPoint;
class ScrollableArea;
enum class ScrollClamping : bool;

class ScrollAnimation {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~ScrollAnimation() { };
    virtual bool scroll(ScrollbarOrientation, ScrollGranularity, float /* step */, float /* multiplier */) { return true; };
    virtual void scroll(const FloatPoint&) { };
    virtual void stop() = 0;
    virtual void updateVisibleLengths() { };
    virtual void setCurrentPosition(const FloatPoint&) { };
    virtual void serviceAnimation() { };

protected:
    ScrollAnimation(ScrollableArea& scrollableArea)
        : m_scrollableArea(scrollableArea)
    {
    }

    ScrollableArea& m_scrollableArea;
};

} // namespace WebCore

#endif // ScrollAnimation_h
