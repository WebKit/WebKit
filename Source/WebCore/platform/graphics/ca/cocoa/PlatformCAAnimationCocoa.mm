/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "PlatformCAAnimationCocoa.h"

#import "FloatConversion.h"
#import "PlatformCAFilters.h"
#import "TimingFunction.h"
#import <QuartzCore/QuartzCore.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/WTFString.h>

namespace WebCore {

constexpr NSString * const WKExplicitBeginTimeFlag = @"WKPlatformCAAnimationExplicitBeginTimeFlag";

bool hasExplicitBeginTime(CAAnimation *animation)
{
    return [[animation valueForKey:WKExplicitBeginTimeFlag] boolValue];
}

void setHasExplicitBeginTime(CAAnimation *animation, bool value)
{
    [animation setValue:[NSNumber numberWithBool:value] forKey:WKExplicitBeginTimeFlag];
}
    
NSString* toCAFillModeType(PlatformCAAnimation::FillModeType type)
{
    switch (type) {
    case PlatformCAAnimation::NoFillMode:
    case PlatformCAAnimation::Forwards: return kCAFillModeForwards;
    case PlatformCAAnimation::Backwards: return kCAFillModeBackwards;
    case PlatformCAAnimation::Both: return kCAFillModeBoth;
    }
    return @"";
}

static PlatformCAAnimation::FillModeType fromCAFillModeType(NSString* string)
{
    if ([string isEqualToString:kCAFillModeBackwards])
        return PlatformCAAnimation::Backwards;

    if ([string isEqualToString:kCAFillModeBoth])
        return PlatformCAAnimation::Both;

    return PlatformCAAnimation::Forwards;
}

NSString* toCAValueFunctionType(PlatformCAAnimation::ValueFunctionType type)
{
    switch (type) {
    case PlatformCAAnimation::NoValueFunction: return @"";
    case PlatformCAAnimation::RotateX: return kCAValueFunctionRotateX;
    case PlatformCAAnimation::RotateY: return kCAValueFunctionRotateY;
    case PlatformCAAnimation::RotateZ: return kCAValueFunctionRotateZ;
    case PlatformCAAnimation::ScaleX: return kCAValueFunctionScaleX;
    case PlatformCAAnimation::ScaleY: return kCAValueFunctionScaleY;
    case PlatformCAAnimation::ScaleZ: return kCAValueFunctionScaleZ;
    case PlatformCAAnimation::Scale: return kCAValueFunctionScale;
    case PlatformCAAnimation::TranslateX: return kCAValueFunctionTranslateX;
    case PlatformCAAnimation::TranslateY: return kCAValueFunctionTranslateY;
    case PlatformCAAnimation::TranslateZ: return kCAValueFunctionTranslateZ;
    case PlatformCAAnimation::Translate: return kCAValueFunctionTranslate;
    }
    return @"";
}

static PlatformCAAnimation::ValueFunctionType fromCAValueFunctionType(NSString* string)
{
    if ([string isEqualToString:kCAValueFunctionRotateX])
        return PlatformCAAnimation::RotateX;

    if ([string isEqualToString:kCAValueFunctionRotateY])
        return PlatformCAAnimation::RotateY;

    if ([string isEqualToString:kCAValueFunctionRotateZ])
        return PlatformCAAnimation::RotateZ;

    if ([string isEqualToString:kCAValueFunctionScaleX])
        return PlatformCAAnimation::ScaleX;

    if ([string isEqualToString:kCAValueFunctionScaleY])
        return PlatformCAAnimation::ScaleY;

    if ([string isEqualToString:kCAValueFunctionScaleZ])
        return PlatformCAAnimation::ScaleZ;

    if ([string isEqualToString:kCAValueFunctionScale])
        return PlatformCAAnimation::Scale;

    if ([string isEqualToString:kCAValueFunctionTranslateX])
        return PlatformCAAnimation::TranslateX;

    if ([string isEqualToString:kCAValueFunctionTranslateY])
        return PlatformCAAnimation::TranslateY;

    if ([string isEqualToString:kCAValueFunctionTranslateZ])
        return PlatformCAAnimation::TranslateZ;

    if ([string isEqualToString:kCAValueFunctionTranslate])
        return PlatformCAAnimation::Translate;

    return PlatformCAAnimation::NoValueFunction;
}

CAMediaTimingFunction* toCAMediaTimingFunction(const TimingFunction* timingFunction, bool reverse)
{
    ASSERT(timingFunction);
    if (is<CubicBezierTimingFunction>(timingFunction)) {
        RefPtr<CubicBezierTimingFunction> reversed;
        auto* function = downcast<CubicBezierTimingFunction>(timingFunction);

        if (reverse) {
            reversed = function->createReversed();
            function = reversed.get();
        }

        float x1 = static_cast<float>(function->x1());
        float y1 = static_cast<float>(function->y1());
        float x2 = static_cast<float>(function->x2());
        float y2 = static_cast<float>(function->y2());
        return [CAMediaTimingFunction functionWithControlPoints: x1 : y1 : x2 : y2];
    }
    
    ASSERT(timingFunction->type() == TimingFunction::LinearFunction);
    return [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionLinear];
}

Ref<PlatformCAAnimation> PlatformCAAnimationCocoa::create(AnimationType type, const String& keyPath)
{
    return adoptRef(*new PlatformCAAnimationCocoa(type, keyPath));
}

Ref<PlatformCAAnimation> PlatformCAAnimationCocoa::create(PlatformAnimationRef animation)
{
    return adoptRef(*new PlatformCAAnimationCocoa(animation));
}

PlatformCAAnimationCocoa::PlatformCAAnimationCocoa(AnimationType type, const String& keyPath)
    : PlatformCAAnimation(type)
{
    switch (type) {
    case Basic:
        m_animation = [CABasicAnimation animationWithKeyPath:keyPath];
        break;
    case Group:
        m_animation = [CAAnimationGroup animation];
        break;
    case Keyframe:
        m_animation = [CAKeyframeAnimation animationWithKeyPath:keyPath];
        break;
    case Spring:
        m_animation = [CASpringAnimation animationWithKeyPath:keyPath];
        break;
    }
}

PlatformCAAnimationCocoa::PlatformCAAnimationCocoa(PlatformAnimationRef animation)
{
    auto caAnimation = static_cast<CAAnimation *>(animation);
    if ([caAnimation isKindOfClass:[CABasicAnimation class]]) {
        if ([caAnimation isKindOfClass:[CASpringAnimation class]])
            setType(Spring);
        else
            setType(Basic);
    } else if ([caAnimation isKindOfClass:[CAKeyframeAnimation class]])
        setType(Keyframe);
    else if ([caAnimation isKindOfClass:[CAAnimationGroup class]])
        setType(Group);
    else {
        ASSERT_NOT_REACHED();
        return;
    }
    
    m_animation = caAnimation;
}

PlatformCAAnimationCocoa::~PlatformCAAnimationCocoa()
{
}

Ref<PlatformCAAnimation> PlatformCAAnimationCocoa::copy() const
{
    auto animation = create(animationType(), keyPath());
    
    animation->setBeginTime(beginTime());
    animation->setDuration(duration());
    animation->setSpeed(speed());
    animation->setTimeOffset(timeOffset());
    animation->setRepeatCount(repeatCount());
    animation->setAutoreverses(autoreverses());
    animation->setFillMode(fillMode());
    animation->setRemovedOnCompletion(isRemovedOnCompletion());
    animation->setAdditive(isAdditive());
    animation->copyTimingFunctionFrom(*this);
    animation->setValueFunction(valueFunction());

    setHasExplicitBeginTime(downcast<PlatformCAAnimationCocoa>(animation.get()).platformAnimation(), hasExplicitBeginTime(platformAnimation()));
    
    // Copy the specific Basic or Keyframe values.
    if (animationType() == Keyframe) {
        animation->copyValuesFrom(*this);
        animation->copyKeyTimesFrom(*this);
        animation->copyTimingFunctionsFrom(*this);
    } else {
        animation->copyFromValueFrom(*this);
        animation->copyToValueFrom(*this);
    }
    
    return animation;
}

PlatformAnimationRef PlatformCAAnimationCocoa::platformAnimation() const
{
    return m_animation.get();
}

String PlatformCAAnimationCocoa::keyPath() const
{
    if (animationType() == Group)
        return emptyString();

    ASSERT([static_cast<CAAnimation *>(m_animation.get()) isKindOfClass:[CAPropertyAnimation class]]);
    return [static_cast<CAPropertyAnimation *>(m_animation.get()) keyPath];
}

CFTimeInterval PlatformCAAnimationCocoa::beginTime() const
{
    return [m_animation beginTime];
}

void PlatformCAAnimationCocoa::setBeginTime(CFTimeInterval value)
{
    [m_animation setBeginTime:value];
    
    // Also set a flag to tell us if we've passed in a 0 value. 
    // The flag is needed because later beginTime will get changed
    // to the time at which it fired and we need to know whether
    // or not it was 0 to begin with.
    if (value)
        setHasExplicitBeginTime(m_animation.get(), true);
}

CFTimeInterval PlatformCAAnimationCocoa::duration() const
{
    return [m_animation duration];
}

void PlatformCAAnimationCocoa::setDuration(CFTimeInterval value)
{
    [m_animation setDuration:value];
}

float PlatformCAAnimationCocoa::speed() const
{
    return [m_animation speed];
}

void PlatformCAAnimationCocoa::setSpeed(float value)
{
    [m_animation setSpeed:value];
}

CFTimeInterval PlatformCAAnimationCocoa::timeOffset() const
{
    return [m_animation timeOffset];
}

void PlatformCAAnimationCocoa::setTimeOffset(CFTimeInterval value)
{
    [m_animation setTimeOffset:value];
}

float PlatformCAAnimationCocoa::repeatCount() const
{
    return [m_animation repeatCount];
}

void PlatformCAAnimationCocoa::setRepeatCount(float value)
{
    [m_animation setRepeatCount:value];
}

bool PlatformCAAnimationCocoa::autoreverses() const
{
    return [m_animation autoreverses];
}

void PlatformCAAnimationCocoa::setAutoreverses(bool value)
{
    [m_animation setAutoreverses:value];
}

PlatformCAAnimation::FillModeType PlatformCAAnimationCocoa::fillMode() const
{
    return fromCAFillModeType([m_animation fillMode]);
}

void PlatformCAAnimationCocoa::setFillMode(FillModeType value)
{
    [m_animation setFillMode:toCAFillModeType(value)];
}

void PlatformCAAnimationCocoa::setTimingFunction(const TimingFunction* value, bool reverse)
{
    switch (animationType()) {
    case Basic:
    case Keyframe:
        [m_animation setTimingFunction:toCAMediaTimingFunction(value, reverse)];
        break;
    case Spring:
        if (value->isSpringTimingFunction()) {
            // FIXME: Handle reverse.
            auto& function = *static_cast<const SpringTimingFunction*>(value);
            CASpringAnimation *springAnimation = (CASpringAnimation *)m_animation.get();
            springAnimation.mass = function.mass();
            springAnimation.stiffness = function.stiffness();
            springAnimation.damping = function.damping();
            springAnimation.initialVelocity = function.initialVelocity();
        }
        break;
    case Group:
        break;
    }
}

void PlatformCAAnimationCocoa::copyTimingFunctionFrom(const PlatformCAAnimation& value)
{
    [m_animation setTimingFunction:[downcast<PlatformCAAnimationCocoa>(value).m_animation.get() timingFunction]];
}

bool PlatformCAAnimationCocoa::isRemovedOnCompletion() const
{
    return [m_animation isRemovedOnCompletion];
}

void PlatformCAAnimationCocoa::setRemovedOnCompletion(bool value)
{
    [m_animation setRemovedOnCompletion:value];
}

bool PlatformCAAnimationCocoa::isAdditive() const
{
    if (animationType() == Group)
        return false;

    ASSERT([static_cast<CAAnimation *>(m_animation.get()) isKindOfClass:[CAPropertyAnimation class]]);
    return [static_cast<CAPropertyAnimation *>(m_animation.get()) isAdditive];
}

void PlatformCAAnimationCocoa::setAdditive(bool value)
{
    if (animationType() == Group)
        return;

    ASSERT([static_cast<CAAnimation *>(m_animation.get()) isKindOfClass:[CAPropertyAnimation class]]);
    return [static_cast<CAPropertyAnimation *>(m_animation.get()) setAdditive:value];
}

PlatformCAAnimation::ValueFunctionType PlatformCAAnimationCocoa::valueFunction() const
{
    if (animationType() == Group)
        return NoValueFunction;

    ASSERT([static_cast<CAAnimation *>(m_animation.get()) isKindOfClass:[CAPropertyAnimation class]]);
    return fromCAValueFunctionType([[static_cast<CAPropertyAnimation *>(m_animation.get()) valueFunction] name]);
}

void PlatformCAAnimationCocoa::setValueFunction(ValueFunctionType value)
{
    if (animationType() == Group)
        return;

    ASSERT([static_cast<CAAnimation *>(m_animation.get()) isKindOfClass:[CAPropertyAnimation class]]);
    return [static_cast<CAPropertyAnimation *>(m_animation.get()) setValueFunction:[CAValueFunction functionWithName:toCAValueFunctionType(value)]];
}

void PlatformCAAnimationCocoa::setFromValue(float value)
{
    if (!isBasicAnimation())
        return;
    [static_cast<CABasicAnimation *>(m_animation.get()) setFromValue:@(value)];
}

void PlatformCAAnimationCocoa::setFromValue(const TransformationMatrix& value)
{
    if (!isBasicAnimation())
        return;
    [static_cast<CABasicAnimation *>(m_animation.get()) setFromValue:[NSValue valueWithCATransform3D:value]];
}

void PlatformCAAnimationCocoa::setFromValue(const FloatPoint3D& value)
{
    if (!isBasicAnimation())
        return;
    [static_cast<CABasicAnimation *>(m_animation.get()) setFromValue:@[ @(value.x()), @(value.y()), @(value.z()) ]];
}

void PlatformCAAnimationCocoa::setFromValue(const Color& value)
{
    if (!isBasicAnimation())
        return;
    auto [r, g, b, a] = value.toSRGBALossy<uint8_t>();
    [static_cast<CABasicAnimation *>(m_animation.get()) setFromValue:@[@(r), @(g), @(b), @(a)]];
}

void PlatformCAAnimationCocoa::setFromValue(const FilterOperation* operation, int internalFilterPropertyIndex)
{
    auto value = PlatformCAFilters::filterValueForOperation(operation, internalFilterPropertyIndex);
    [static_cast<CABasicAnimation *>(m_animation.get()) setFromValue:value.get()];
}

void PlatformCAAnimationCocoa::copyFromValueFrom(const PlatformCAAnimation& value)
{
    if (!isBasicAnimation() || !value.isBasicAnimation())
        return;
    auto otherAnimation = static_cast<CABasicAnimation *>(downcast<PlatformCAAnimationCocoa>(value).m_animation.get());
    [static_cast<CABasicAnimation *>(m_animation.get()) setFromValue:[otherAnimation fromValue]];
}

void PlatformCAAnimationCocoa::setToValue(float value)
{
    if (!isBasicAnimation())
        return;
    [static_cast<CABasicAnimation *>(m_animation.get()) setToValue:@(value)];
}

void PlatformCAAnimationCocoa::setToValue(const TransformationMatrix& value)
{
    if (!isBasicAnimation())
        return;
    [static_cast<CABasicAnimation *>(m_animation.get()) setToValue:[NSValue valueWithCATransform3D:value]];
}

void PlatformCAAnimationCocoa::setToValue(const FloatPoint3D& value)
{
    if (!isBasicAnimation())
        return;
    [static_cast<CABasicAnimation *>(m_animation.get()) setToValue:@[ @(value.x()), @(value.y()), @(value.z()) ]];
}

void PlatformCAAnimationCocoa::setToValue(const Color& value)
{
    if (!isBasicAnimation())
        return;
    auto [r, g, b, a] = value.toSRGBALossy<uint8_t>();
    [static_cast<CABasicAnimation *>(m_animation.get()) setToValue:@[@(r), @(g), @(b), @(a)]];
}

void PlatformCAAnimationCocoa::setToValue(const FilterOperation* operation, int internalFilterPropertyIndex)
{
    auto value = PlatformCAFilters::filterValueForOperation(operation, internalFilterPropertyIndex);
    [static_cast<CABasicAnimation *>(m_animation.get()) setToValue:value.get()];
}

void PlatformCAAnimationCocoa::copyToValueFrom(const PlatformCAAnimation& value)
{
    if (!isBasicAnimation() || !value.isBasicAnimation())
        return;

    auto otherAnimation = static_cast<CABasicAnimation *>(downcast<PlatformCAAnimationCocoa>(value).m_animation.get());
    [static_cast<CABasicAnimation *>(m_animation.get()) setToValue:[otherAnimation toValue]];
}


// Keyframe-animation properties.
void PlatformCAAnimationCocoa::setValues(const Vector<float>& value)
{
    if (animationType() != Keyframe)
        return;

    [static_cast<CAKeyframeAnimation *>(m_animation.get()) setValues:createNSArray(value, [] (float number) {
        return @(number);
    }).get()];
}

void PlatformCAAnimationCocoa::setValues(const Vector<TransformationMatrix>& value)
{
    if (animationType() != Keyframe)
        return;

    [static_cast<CAKeyframeAnimation *>(m_animation.get()) setValues:createNSArray(value, [] (auto& matrix) {
        return [NSValue valueWithCATransform3D:matrix];
    }).get()];
}

void PlatformCAAnimationCocoa::setValues(const Vector<FloatPoint3D>& value)
{
    if (animationType() != Keyframe)
        return;

    [static_cast<CAKeyframeAnimation *>(m_animation.get()) setValues:createNSArray(value, [] (auto& point) {
        return @[@(point.x()), @(point.y()), @(point.z())];
    }).get()];
}

void PlatformCAAnimationCocoa::setValues(const Vector<Color>& value)
{
    if (animationType() != Keyframe)
        return;

    [static_cast<CAKeyframeAnimation *>(m_animation.get()) setValues:createNSArray(value, [] (auto& color) {
        auto [r, g, b, a] = color.template toSRGBALossy<uint8_t>();
        return @[@(r), @(g), @(b), @(a)];
    }).get()];
}

void PlatformCAAnimationCocoa::setValues(const Vector<RefPtr<FilterOperation>>& values, int internalFilterPropertyIndex)
{
    if (animationType() != Keyframe)
        return;

    [static_cast<CAKeyframeAnimation *>(m_animation.get()) setValues:createNSArray(values, [&] (auto& value) {
        return PlatformCAFilters::filterValueForOperation(value.get(), internalFilterPropertyIndex);
    }).get()];
}

void PlatformCAAnimationCocoa::copyValuesFrom(const PlatformCAAnimation& value)
{
    if (animationType() != Keyframe || value.animationType() != Keyframe)
        return;

    auto otherAnimation = static_cast<CAKeyframeAnimation *>(downcast<PlatformCAAnimationCocoa>(value).m_animation.get());
    [static_cast<CAKeyframeAnimation *>(m_animation.get()) setValues:[otherAnimation values]];
}

void PlatformCAAnimationCocoa::setKeyTimes(const Vector<float>& value)
{
    [static_cast<CAKeyframeAnimation *>(m_animation.get()) setKeyTimes:createNSArray(value, [] (float time) {
        return @(time);
    }).get()];
}

void PlatformCAAnimationCocoa::copyKeyTimesFrom(const PlatformCAAnimation& value)
{
    auto other = static_cast<CAKeyframeAnimation *>(downcast<PlatformCAAnimationCocoa>(value).m_animation.get());
    [static_cast<CAKeyframeAnimation *>(m_animation.get()) setKeyTimes:[other keyTimes]];
}

void PlatformCAAnimationCocoa::setTimingFunctions(const Vector<const TimingFunction*>& value, bool reverse)
{
    [static_cast<CAKeyframeAnimation *>(m_animation.get()) setTimingFunctions:createNSArray(value, [&] (auto& function) {
        return toCAMediaTimingFunction(function, reverse);
    }).get()];
}

void PlatformCAAnimationCocoa::copyTimingFunctionsFrom(const PlatformCAAnimation& value)
{
    auto other = static_cast<CAKeyframeAnimation *>(downcast<PlatformCAAnimationCocoa>(value).m_animation.get());
    [static_cast<CAKeyframeAnimation *>(m_animation.get()) setTimingFunctions:[other timingFunctions]];
}

void PlatformCAAnimationCocoa::setAnimations(const Vector<RefPtr<PlatformCAAnimation>>& value)
{
    ASSERT(animationType() == Group);
    ASSERT([static_cast<CAAnimation *>(m_animation.get()) isKindOfClass:[CAAnimationGroup class]]);

    [static_cast<CAAnimationGroup *>(m_animation.get()) setAnimations:createNSArray(value, [&] (auto& animation) -> CAAnimation * {
        if (is<PlatformCAAnimationCocoa>(animation.get()))
            return downcast<PlatformCAAnimationCocoa>(animation.get())->m_animation.get();
        return nil;
    }).get()];
}

void PlatformCAAnimationCocoa::copyAnimationsFrom(const PlatformCAAnimation& value)
{
    ASSERT(animationType() == Group);
    ASSERT(value.animationType() == Group);
    ASSERT([static_cast<CAAnimation *>(m_animation.get()) isKindOfClass:[CAAnimationGroup class]]);
    ASSERT([static_cast<CAAnimation *>(downcast<PlatformCAAnimationCocoa>(value).m_animation.get()) isKindOfClass:[CAAnimationGroup class]]);

    auto other = static_cast<CAAnimationGroup *>(downcast<PlatformCAAnimationCocoa>(value).m_animation.get());
    [static_cast<CAAnimationGroup *>(m_animation.get()) setAnimations:[other animations]];
}

} // namespace WebCore
