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
#include "DocumentAnimation.h"

#if ENABLE(WEB_ANIMATIONS)

#include "Document.h"
#include "DocumentTimeline.h"

namespace WebCore {

DocumentAnimation::DocumentAnimation()
{
}

DocumentAnimation::~DocumentAnimation()
{
}

DocumentTimeline* DocumentAnimation::timeline(Document& document)
{
    auto* documentAnimation = DocumentAnimation::from(&document);
    if (!documentAnimation->m_defaultTimeline)
        documentAnimation->m_defaultTimeline = DocumentTimeline::create(document, 0.0);
    return documentAnimation->m_defaultTimeline.get();
}

const char* DocumentAnimation::supplementName()
{
    return "DocumentAnimation";
}

DocumentAnimation* DocumentAnimation::from(Document* document)
{
    DocumentAnimation* supplement = static_cast<DocumentAnimation*>(Supplement<Document>::from(document, supplementName()));
    if (!supplement) {
        auto newSupplement = std::make_unique<DocumentAnimation>();
        supplement = newSupplement.get();
        provideTo(document, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

WebAnimationVector DocumentAnimation::getAnimations(const WTF::Function<bool(const AnimationEffect&)>& effectMatches) const
{
    WebAnimationVector animations;

    auto sortBasedOnPriority = [](const RefPtr<WebAnimation>& a, const RefPtr<WebAnimation>& b)
    {
        // FIXME: Sort using the composite order as described in the spec.
        UNUSED_PARAM(a);
        UNUSED_PARAM(b);
        return true;
    };

    for (auto& animation : m_animations.values()) {
        if (animation && animation->effect()) {
            const AnimationEffect& effect = *animation->effect();
            if ((effect.isCurrent() || effect.isInEffect()) && effectMatches(effect))
                animations.append(animation.get());
        }
    }
    std::sort(animations.begin(), animations.end(), sortBasedOnPriority);

    return animations;
}

void DocumentAnimation::addAnimation(WebAnimation& animation)
{
    ASSERT(!m_animations.contains(&animation));
    m_animations.add(&animation, animation.createWeakPtr());
}

void DocumentAnimation::removeAnimation(WebAnimation& animation)
{
    m_animations.remove(&animation);
}

WebAnimationVector DocumentAnimation::getAnimations(Document& document)
{
    return DocumentAnimation::from(&document)->getAnimations();
}

} // namespace WebCore

#endif // ENABLE(WEB_ANIMATIONS)
