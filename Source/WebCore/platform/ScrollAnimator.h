/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScrollAnimator_h
#define ScrollAnimator_h

#include "ScrollTypes.h"

namespace WebCore {

class ScrollbarClient;

class ScrollAnimator {
public:
    static ScrollAnimator* create(ScrollbarClient*);

    ScrollAnimator(ScrollbarClient* client);
    virtual ~ScrollAnimator();

    // Computes a scroll destination for the given parameters.  Returns false if
    // already at the destination.  Otherwise, starts scrolling towards the
    // destination and returns true.  Scrolling may be immediate or animated.
    // The base class implementation always scrolls immediately, never animates.
    virtual bool scroll(ScrollbarOrientation, ScrollGranularity, float step, float multiplier);

    // Stops any animation in the given direction and updates the ScrollAnimator
    // with the current scroll position.  This does not cause a callback to the
    // ScrollbarClient.
    virtual void setScrollPositionAndStopAnimation(ScrollbarOrientation, float);

protected:
    ScrollbarClient* m_client;
    float m_currentPosX; // We avoid using a FloatPoint in order to reduce
    float m_currentPosY; // subclass code complexity.
};

} // namespace WebCore
#endif
