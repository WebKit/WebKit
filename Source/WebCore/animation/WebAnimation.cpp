/*
 * Copyright (C) Canon Inc. 2016
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Canon Inc. nor the names of
 * its contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebAnimation.h"

#if ENABLE(WEB_ANIMATIONS)

#include "AnimationEffect.h"
#include "DocumentTimeline.h"
#include <wtf/Ref.h>

namespace WebCore {

RefPtr<WebAnimation> WebAnimation::create(AnimationEffect* effect, AnimationTimeline* timeline)
{
    if (!effect) {
        // FIXME: Support creating animations with null effect.
        return nullptr;
    }

    if (!timeline) {
        // FIXME: Support creating animations without a timeline.
        return nullptr;
    }

    if (!timeline->isDocumentTimeline()) {
        // FIXME: Currently only support DocumentTimeline.
        return nullptr;
    }

    return adoptRef(new WebAnimation(effect, timeline));
}

WebAnimation::WebAnimation(AnimationEffect* effect, AnimationTimeline* timeline)
    : m_effect(effect)
    , m_timeline(timeline)
    , m_weakPtrFactory(this)
{
    if (m_effect)
        m_effect->setAnimation(this);

    if (m_timeline)
        m_timeline->attachAnimation(*this);
}

WebAnimation::~WebAnimation()
{
    if (m_effect)
        m_effect->setAnimation(nullptr);

    if (m_timeline)
        m_timeline->detachAnimation(*this);
}

} // namespace WebCore

#endif // ENABLE(WEB_ANIMATIONS)
