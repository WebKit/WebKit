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

#include "config.h"
#include "KeyframeEffect.h"

#include "Animation.h"
#include "CSSPropertyAnimation.h"
#include "Element.h"
#include "RenderStyle.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "WillChangeData.h"
#include <wtf/UUID.h>

namespace WebCore {
using namespace JSC;

ExceptionOr<Ref<KeyframeEffect>> KeyframeEffect::create(ExecState& state, Element* target, Strong<JSObject>&& keyframes)
{
    auto keyframeEffect = adoptRef(*new KeyframeEffect(target));

    auto setKeyframesResult = keyframeEffect->setKeyframes(state, WTFMove(keyframes));
    if (setKeyframesResult.hasException())
        return setKeyframesResult.releaseException();

    return WTFMove(keyframeEffect);
}

KeyframeEffect::KeyframeEffect(Element* target)
    : AnimationEffect(KeyframeEffectClass)
    , m_target(target)
    , m_keyframes(emptyString())
{
}

ExceptionOr<void> KeyframeEffect::setKeyframes(ExecState& state, Strong<JSObject>&& keyframes)
{
    auto processKeyframesResult = processKeyframes(state, WTFMove(keyframes));
    if (processKeyframesResult.hasException())
        return processKeyframesResult.releaseException();
    return { };
}

ExceptionOr<void> KeyframeEffect::processKeyframes(ExecState& state, Strong<JSObject>&& keyframes)
{
    // FIXME: We only have primitive to-from parsing, for full support see webkit.org/b/179708.
    // Full specification is at https://w3c.github.io/web-animations/#processing-a-keyframes-argument.

    if (!m_target || !keyframes)
        return { };

    if (!isJSArray(keyframes.get()))
        return Exception { TypeError };

    KeyframeList newKeyframes("keyframe-effect-" + createCanonicalUUIDString());

    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    StyleResolver& styleResolver = m_target->styleResolver();
    auto parserContext = CSSParserContext(HTMLStandardMode);

    auto* array = jsCast<const JSArray*>(keyframes.get());
    auto length = array->length();
    if (length != 2)
        return Exception { TypeError };

    for (unsigned i = 0; i < length; ++i) {
        const JSValue value = array->getIndex(&state, i);
        if (scope.exception() || !value || !value.isObject())
            return Exception { TypeError };
        JSObject* keyframe = value.toObject(&state);
        PropertyNameArray ownPropertyNames(&vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
        JSObject::getOwnPropertyNames(keyframe, &state, ownPropertyNames, EnumerationMode());
        size_t numberOfProperties = ownPropertyNames.size();

        StringBuilder cssText;
        for (size_t j = 0; j < numberOfProperties; ++j) {
            cssText.append(ownPropertyNames[j].string());
            cssText.appendLiteral(": ");
            cssText.append(keyframe->get(&state, ownPropertyNames[j]).toWTFString(&state));
            cssText.appendLiteral("; ");
        }

        auto renderStyle = RenderStyle::createPtr();
        auto styleProperties = MutableStyleProperties::create();
        styleProperties->parseDeclaration(cssText.toString(), parserContext);
        unsigned numberOfCSSProperties = styleProperties->propertyCount();

        KeyframeValue keyframeValue(0, nullptr);
        for (unsigned k = 0; k < numberOfCSSProperties; ++k) {
            auto cssPropertyId = styleProperties->propertyAt(k).id();
            keyframeValue.addProperty(cssPropertyId);
            newKeyframes.addProperty(cssPropertyId);
            styleResolver.applyPropertyToStyle(cssPropertyId, styleProperties->propertyAt(k).value(), WTFMove(renderStyle));
            renderStyle = styleResolver.state().takeStyle();
        }

        keyframeValue.setKey(i);
        keyframeValue.setStyle(RenderStyle::clonePtr(*renderStyle));
        newKeyframes.insert(WTFMove(keyframeValue));
    }

    m_keyframes = WTFMove(newKeyframes);

    computeStackingContextImpact();

    return { };
}

void KeyframeEffect::computeStackingContextImpact()
{
    m_triggersStackingContext = false;
    for (auto cssPropertyId : m_keyframes.properties()) {
        if (WillChangeData::propertyCreatesStackingContext(cssPropertyId)) {
            m_triggersStackingContext = true;
            break;
        }
    }
}

void KeyframeEffect::applyAtLocalTime(Seconds localTime, RenderStyle& targetStyle)
{
    if (!m_target)
        return;

    if (m_startedAccelerated && localTime >= timing()->duration()) {
        m_startedAccelerated = false;
        animation()->acceleratedRunningStateDidChange();
    }

    // FIXME: Assume animations only apply in the range [0, duration[
    // until we support fill modes, delays and iterations.
    if (localTime < 0_s || localTime >= timing()->duration())
        return;

    if (!timing()->duration())
        return;

    bool needsToStartAccelerated = false;

    if (!m_started && !m_startedAccelerated) {
        needsToStartAccelerated = shouldRunAccelerated();
        m_startedAccelerated = needsToStartAccelerated;
        if (needsToStartAccelerated)
            animation()->acceleratedRunningStateDidChange();
    }
    m_started = true;

    if (!needsToStartAccelerated && !m_startedAccelerated) {
        float progress = localTime / timing()->duration();
        for (auto cssPropertyId : m_keyframes.properties())
            CSSPropertyAnimation::blendProperties(this, cssPropertyId, &targetStyle, m_keyframes[0].style(), m_keyframes[1].style(), progress);
    }

    // https://w3c.github.io/web-animations/#side-effects-section
    // For every property targeted by at least one animation effect that is current or in effect, the user agent
    // must act as if the will-change property ([css-will-change-1]) on the target element includes the property.
    if (m_triggersStackingContext && targetStyle.hasAutoZIndex())
        targetStyle.setZIndex(0);
}

bool KeyframeEffect::shouldRunAccelerated()
{
    for (auto cssPropertyId : m_keyframes.properties()) {
        if (!CSSPropertyAnimation::animationOfPropertyIsAccelerated(cssPropertyId))
            return false;
    }
    return true;
}

void KeyframeEffect::getAnimatedStyle(std::unique_ptr<RenderStyle>& animatedStyle)
{
    if (!animation() || !timing()->duration())
        return;

    auto localTime = animation()->currentTime();

    // FIXME: Assume animations only apply in the range [0, duration[
    // until we support fill modes, delays and iterations.
    if (!localTime || localTime < 0_s || localTime >= timing()->duration())
        return;

    if (!m_keyframes.size())
        return;

    if (!animatedStyle)
        animatedStyle = RenderStyle::clonePtr(renderer()->style());

    for (auto cssPropertyId : m_keyframes.properties()) {
        float progress = localTime.value() / timing()->duration();
        CSSPropertyAnimation::blendProperties(this, cssPropertyId, animatedStyle.get(), m_keyframes[0].style(), m_keyframes[1].style(), progress);
    }
}

void KeyframeEffect::startOrStopAccelerated()
{
    auto* renderer = this->renderer();
    if (!renderer || !renderer->isComposited())
        return;

    auto* compositedRenderer = downcast<RenderBoxModelObject>(renderer);
    if (m_startedAccelerated) {
        auto animation = Animation::create();
        animation->setDuration(timing()->duration().value());
        compositedRenderer->startAnimation(0, animation.ptr(), m_keyframes);
    } else {
        compositedRenderer->animationFinished(m_keyframes.animationName());
        if (!m_target->document().renderTreeBeingDestroyed())
            m_target->invalidateStyleAndLayerComposition();
    }
}

RenderElement* KeyframeEffect::renderer() const
{
    return m_target ? m_target->renderer() : nullptr;
}

const RenderStyle& KeyframeEffect::currentStyle() const
{
    if (auto* renderer = this->renderer())
        return renderer->style();
    return RenderStyle::defaultStyle();
}

} // namespace WebCore
