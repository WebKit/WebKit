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

#include "config.h"

#import "PlatformCAAnimationMac.h"

#import "FloatConversion.h"
#import "PlatformCAFilters.h"
#import "TimingFunction.h"
#import <QuartzCore/QuartzCore.h>
#import <wtf/text/WTFString.h>

using namespace WebCore;

// This value must be the same as in PlatformCALayerMac.mm
static NSString * const WKNonZeroBeginTimeFlag = @"WKPlatformCAAnimationNonZeroBeginTimeFlag";

static bool hasNonZeroBeginTimeFlag(const PlatformCAAnimation* animation)
{
    return [[toPlatformCAAnimationMac(animation)->platformAnimation() valueForKey:WKNonZeroBeginTimeFlag] boolValue];
}

static void setNonZeroBeginTimeFlag(PlatformCAAnimation* animation, bool value)
{
    [toPlatformCAAnimationMac(animation)->platformAnimation() setValue:[NSNumber numberWithBool:value] forKey:WKNonZeroBeginTimeFlag];
}
    
NSString* WebCore::toCAFillModeType(PlatformCAAnimation::FillModeType type)
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

NSString* WebCore::toCAValueFunctionType(PlatformCAAnimation::ValueFunctionType type)
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

CAMediaTimingFunction* WebCore::toCAMediaTimingFunction(const TimingFunction* timingFunction, bool reverse)
{
    ASSERT(timingFunction);
    if (timingFunction->isCubicBezierTimingFunction()) {
        RefPtr<CubicBezierTimingFunction> reversed;
        const CubicBezierTimingFunction* ctf = static_cast<const CubicBezierTimingFunction*>(timingFunction);

        if (reverse) {
            reversed = ctf->createReversed();
            ctf = reversed.get();
        }

        float x1 = static_cast<float>(ctf->x1());
        float y1 = static_cast<float>(ctf->y1());
        float x2 = static_cast<float>(ctf->x2());
        float y2 = static_cast<float>(ctf->y2());
        return [CAMediaTimingFunction functionWithControlPoints: x1 : y1 : x2 : y2];
    }
    
    ASSERT(timingFunction->type() == TimingFunction::LinearFunction);
    return [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionLinear];
}

PassRefPtr<PlatformCAAnimation> PlatformCAAnimationMac::create(AnimationType type, const String& keyPath)
{
    return adoptRef(new PlatformCAAnimationMac(type, keyPath));
}

PassRefPtr<PlatformCAAnimation> PlatformCAAnimationMac::create(PlatformAnimationRef animation)
{
    return adoptRef(new PlatformCAAnimationMac(animation));
}

PlatformCAAnimationMac::PlatformCAAnimationMac(AnimationType type, const String& keyPath)
    : PlatformCAAnimation(type)
{
    if (type == Basic)
        m_animation = [CABasicAnimation animationWithKeyPath:keyPath];
    else
        m_animation = [CAKeyframeAnimation animationWithKeyPath:keyPath];
}

PlatformCAAnimationMac::PlatformCAAnimationMac(PlatformAnimationRef animation)
{
    if ([static_cast<CAAnimation*>(animation) isKindOfClass:[CABasicAnimation class]])
        setType(Basic);
    else if ([static_cast<CAAnimation*>(animation) isKindOfClass:[CAKeyframeAnimation class]])
        setType(Keyframe);
    else {
        ASSERT(0);
        return;
    }
    
    m_animation = static_cast<CAPropertyAnimation*>(animation);
}

PlatformCAAnimationMac::~PlatformCAAnimationMac()
{
}

PassRefPtr<PlatformCAAnimation> PlatformCAAnimationMac::copy() const
{
    RefPtr<PlatformCAAnimation> animation = create(animationType(), keyPath());
    
    animation->setBeginTime(beginTime());
    animation->setDuration(duration());
    animation->setSpeed(speed());
    animation->setTimeOffset(timeOffset());
    animation->setRepeatCount(repeatCount());
    animation->setAutoreverses(autoreverses());
    animation->setFillMode(fillMode());
    animation->setRemovedOnCompletion(isRemovedOnCompletion());
    animation->setAdditive(isAdditive());
    animation->copyTimingFunctionFrom(this);
    animation->setValueFunction(valueFunction());

    setNonZeroBeginTimeFlag(animation.get(), hasNonZeroBeginTimeFlag(this));
    
    // Copy the specific Basic or Keyframe values
    if (animationType() == Keyframe) {
        animation->copyValuesFrom(this);
        animation->copyKeyTimesFrom(this);
        animation->copyTimingFunctionsFrom(this);
    } else {
        animation->copyFromValueFrom(this);
        animation->copyToValueFrom(this);
    }
    
    return animation;
}

PlatformAnimationRef PlatformCAAnimationMac::platformAnimation() const
{
    return m_animation.get();
}

String PlatformCAAnimationMac::keyPath() const
{
    return [m_animation.get() keyPath];
}

CFTimeInterval PlatformCAAnimationMac::beginTime() const
{
    return [m_animation.get() beginTime];
}

void PlatformCAAnimationMac::setBeginTime(CFTimeInterval value)
{
    [m_animation.get() setBeginTime:value];
    
    // Also set a flag to tell us if we've passed in a 0 value. 
    // The flag is needed because later beginTime will get changed
    // to the time at which it fired and we need to know whether
    // or not it was 0 to begin with.
    if (value)
        setNonZeroBeginTimeFlag(this, true);
}

CFTimeInterval PlatformCAAnimationMac::duration() const
{
    return [m_animation.get() duration];
}

void PlatformCAAnimationMac::setDuration(CFTimeInterval value)
{
    [m_animation.get() setDuration:value];
}

float PlatformCAAnimationMac::speed() const
{
    return [m_animation.get() speed];
}

void PlatformCAAnimationMac::setSpeed(float value)
{
    [m_animation.get() setSpeed:value];
}

CFTimeInterval PlatformCAAnimationMac::timeOffset() const
{
    return [m_animation.get() timeOffset];
}

void PlatformCAAnimationMac::setTimeOffset(CFTimeInterval value)
{
    [m_animation.get() setTimeOffset:value];
}

float PlatformCAAnimationMac::repeatCount() const
{
    return [m_animation.get() repeatCount];
}

void PlatformCAAnimationMac::setRepeatCount(float value)
{
    [m_animation.get() setRepeatCount:value];
}

bool PlatformCAAnimationMac::autoreverses() const
{
    return [m_animation.get() autoreverses];
}

void PlatformCAAnimationMac::setAutoreverses(bool value)
{
    [m_animation.get() setAutoreverses:value];
}

PlatformCAAnimation::FillModeType PlatformCAAnimationMac::fillMode() const
{
    return fromCAFillModeType([m_animation.get() fillMode]);
}

void PlatformCAAnimationMac::setFillMode(FillModeType value)
{
    [m_animation.get() setFillMode:toCAFillModeType(value)];
}

void PlatformCAAnimationMac::setTimingFunction(const TimingFunction* value, bool reverse)
{
    [m_animation.get() setTimingFunction:toCAMediaTimingFunction(value, reverse)];
}

void PlatformCAAnimationMac::copyTimingFunctionFrom(const PlatformCAAnimation* value)
{
    [m_animation.get() setTimingFunction:[toPlatformCAAnimationMac(value)->m_animation.get() timingFunction]];
}

bool PlatformCAAnimationMac::isRemovedOnCompletion() const
{
    return [m_animation.get() isRemovedOnCompletion];
}

void PlatformCAAnimationMac::setRemovedOnCompletion(bool value)
{
    [m_animation.get() setRemovedOnCompletion:value];
}

bool PlatformCAAnimationMac::isAdditive() const
{
    return [m_animation.get() isAdditive];
}

void PlatformCAAnimationMac::setAdditive(bool value)
{
    [m_animation.get() setAdditive:value];
}

PlatformCAAnimation::ValueFunctionType PlatformCAAnimationMac::valueFunction() const
{
    CAValueFunction* vf = [m_animation.get() valueFunction];
    return fromCAValueFunctionType([vf name]);
}

void PlatformCAAnimationMac::setValueFunction(ValueFunctionType value)
{
    [m_animation.get() setValueFunction:[CAValueFunction functionWithName:toCAValueFunctionType(value)]];
}

void PlatformCAAnimationMac::setFromValue(float value)
{
    if (animationType() != Basic)
        return;
    [static_cast<CABasicAnimation*>(m_animation.get()) setFromValue:[NSNumber numberWithDouble:value]];
}

void PlatformCAAnimationMac::setFromValue(const WebCore::TransformationMatrix& value)
{
    if (animationType() != Basic)
        return;
        
    [static_cast<CABasicAnimation*>(m_animation.get()) setFromValue:[NSValue valueWithCATransform3D:value]];
}

void PlatformCAAnimationMac::setFromValue(const FloatPoint3D& value)
{
    if (animationType() != Basic)
        return;

    NSArray* array = [NSArray arrayWithObjects:
                        [NSNumber numberWithDouble:value.x()],
                        [NSNumber numberWithDouble:value.y()],
                        [NSNumber numberWithDouble:value.z()],
                        nil];
    [static_cast<CABasicAnimation*>(m_animation.get()) setFromValue:array];
}

void PlatformCAAnimationMac::setFromValue(const WebCore::Color& value)
{
    if (animationType() != Basic)
        return;

    NSArray* array = [NSArray arrayWithObjects:
                        [NSNumber numberWithDouble:value.red()],
                        [NSNumber numberWithDouble:value.green()],
                        [NSNumber numberWithDouble:value.blue()],
                        [NSNumber numberWithDouble:value.alpha()],
                        nil];
    [static_cast<CABasicAnimation*>(m_animation.get()) setFromValue:array];
}

#if ENABLE(CSS_FILTERS)
void PlatformCAAnimationMac::setFromValue(const FilterOperation* operation, int internalFilterPropertyIndex)
{
    RetainPtr<id> value = PlatformCAFilters::filterValueForOperation(operation, internalFilterPropertyIndex);
    [static_cast<CABasicAnimation*>(m_animation.get()) setFromValue:value.get()];
}
#endif

void PlatformCAAnimationMac::copyFromValueFrom(const PlatformCAAnimation* value)
{
    if (animationType() != Basic || value->animationType() != Basic)
        return;
        
    CABasicAnimation* otherAnimation = static_cast<CABasicAnimation*>(toPlatformCAAnimationMac(value)->m_animation.get());
    [static_cast<CABasicAnimation*>(m_animation.get()) setFromValue:[otherAnimation fromValue]];
}

void PlatformCAAnimationMac::setToValue(float value)
{
    if (animationType() != Basic)
        return;
    [static_cast<CABasicAnimation*>(m_animation.get()) setToValue:[NSNumber numberWithDouble:value]];
}

void PlatformCAAnimationMac::setToValue(const WebCore::TransformationMatrix& value)
{
    if (animationType() != Basic)
        return;

    [static_cast<CABasicAnimation*>(m_animation.get()) setToValue:[NSValue valueWithCATransform3D:value]];
}

void PlatformCAAnimationMac::setToValue(const FloatPoint3D& value)
{
    if (animationType() != Basic)
        return;

    NSArray* array = [NSArray arrayWithObjects:
                        [NSNumber numberWithDouble:value.x()],
                        [NSNumber numberWithDouble:value.y()],
                        [NSNumber numberWithDouble:value.z()],
                        nil];
    [static_cast<CABasicAnimation*>(m_animation.get()) setToValue:array];
}

void PlatformCAAnimationMac::setToValue(const WebCore::Color& value)
{
    if (animationType() != Basic)
        return;

    NSArray* array = [NSArray arrayWithObjects:
                        [NSNumber numberWithDouble:value.red()],
                        [NSNumber numberWithDouble:value.green()],
                        [NSNumber numberWithDouble:value.blue()],
                        [NSNumber numberWithDouble:value.alpha()],
                        nil];
    [static_cast<CABasicAnimation*>(m_animation.get()) setToValue:array];
}

#if ENABLE(CSS_FILTERS)
void PlatformCAAnimationMac::setToValue(const FilterOperation* operation, int internalFilterPropertyIndex)
{
    RetainPtr<id> value = PlatformCAFilters::filterValueForOperation(operation, internalFilterPropertyIndex);
    [static_cast<CABasicAnimation*>(m_animation.get()) setToValue:value.get()];
}
#endif

void PlatformCAAnimationMac::copyToValueFrom(const PlatformCAAnimation* value)
{
    if (animationType() != Basic || value->animationType() != Basic)
        return;
        
    CABasicAnimation* otherAnimation = static_cast<CABasicAnimation*>(toPlatformCAAnimationMac(value)->m_animation.get());
    [static_cast<CABasicAnimation*>(m_animation.get()) setToValue:[otherAnimation toValue]];
}


// Keyframe-animation properties.
void PlatformCAAnimationMac::setValues(const Vector<float>& value)
{
    if (animationType() != Keyframe)
        return;
        
    NSMutableArray* array = [NSMutableArray array];
    for (size_t i = 0; i < value.size(); ++i)
        [array addObject:[NSNumber numberWithDouble:value[i]]];
    [static_cast<CAKeyframeAnimation*>(m_animation.get()) setValues:array];
}

void PlatformCAAnimationMac::setValues(const Vector<WebCore::TransformationMatrix>& value)
{
    if (animationType() != Keyframe)
        return;
        
    NSMutableArray* array = [NSMutableArray array];

    for (size_t i = 0; i < value.size(); ++i)
        [array addObject:[NSValue valueWithCATransform3D:value[i]]];
        
    [static_cast<CAKeyframeAnimation*>(m_animation.get()) setValues:array];
}

void PlatformCAAnimationMac::setValues(const Vector<FloatPoint3D>& value)
{
    if (animationType() != Keyframe)
        return;
        
    NSMutableArray* array = [NSMutableArray array];

    for (size_t i = 0; i < value.size(); ++i) {
        NSArray* object = [NSArray arrayWithObjects:
                        [NSNumber numberWithDouble:value[i].x()],
                        [NSNumber numberWithDouble:value[i].y()],
                        [NSNumber numberWithDouble:value[i].z()],
                        nil];
        [array addObject:object];
    }
    [static_cast<CAKeyframeAnimation*>(m_animation.get()) setValues:array];
}

void PlatformCAAnimationMac::setValues(const Vector<WebCore::Color>& value)
{
    if (animationType() != Keyframe)
        return;
        
    NSMutableArray* array = [NSMutableArray array];

    for (size_t i = 0; i < value.size(); ++i) {
        NSArray* object = [NSArray arrayWithObjects:
                        [NSNumber numberWithDouble:value[i].red()],
                        [NSNumber numberWithDouble:value[i].green()],
                        [NSNumber numberWithDouble:value[i].blue()],
                        [NSNumber numberWithDouble:value[i].alpha()],
                        nil];
        [array addObject:object];
    }
    [static_cast<CAKeyframeAnimation*>(m_animation.get()) setValues:array];
}

#if ENABLE(CSS_FILTERS)
void PlatformCAAnimationMac::setValues(const Vector<RefPtr<FilterOperation>>& values, int internalFilterPropertyIndex)
{
    if (animationType() != Keyframe)
        return;
        
    NSMutableArray* array = [NSMutableArray array];

    for (size_t i = 0; i < values.size(); ++i) {
        RetainPtr<id> value = PlatformCAFilters::filterValueForOperation(values[i].get(), internalFilterPropertyIndex);
        [array addObject:value.get()];
    }
    [static_cast<CAKeyframeAnimation*>(m_animation.get()) setValues:array];
}
#endif

void PlatformCAAnimationMac::copyValuesFrom(const PlatformCAAnimation* value)
{
    if (animationType() != Keyframe || value->animationType() != Keyframe)
        return;
        
    CAKeyframeAnimation* otherAnimation = static_cast<CAKeyframeAnimation*>(toPlatformCAAnimationMac(value)->m_animation.get());
    [static_cast<CAKeyframeAnimation*>(m_animation.get()) setValues:[otherAnimation values]];
}

void PlatformCAAnimationMac::setKeyTimes(const Vector<float>& value)
{
    NSMutableArray* array = [NSMutableArray array];

    for (size_t i = 0; i < value.size(); ++i)
        [array addObject:[NSNumber numberWithFloat:value[i]]];
    
    [static_cast<CAKeyframeAnimation*>(m_animation.get()) setKeyTimes:array];
}

void PlatformCAAnimationMac::copyKeyTimesFrom(const PlatformCAAnimation* value)
{
    CAKeyframeAnimation* other = static_cast<CAKeyframeAnimation*>(toPlatformCAAnimationMac(value)->m_animation.get());
    [static_cast<CAKeyframeAnimation*>(m_animation.get()) setKeyTimes:[other keyTimes]];
}

void PlatformCAAnimationMac::setTimingFunctions(const Vector<const TimingFunction*>& value, bool reverse)
{
    NSMutableArray* array = [NSMutableArray array];

    for (size_t i = 0; i < value.size(); ++i)
        [array addObject:toCAMediaTimingFunction(value[i], reverse)];
    
    [static_cast<CAKeyframeAnimation*>(m_animation.get()) setTimingFunctions:array];
}

void PlatformCAAnimationMac::copyTimingFunctionsFrom(const PlatformCAAnimation* value)
{
    CAKeyframeAnimation* other = static_cast<CAKeyframeAnimation*>(toPlatformCAAnimationMac(value)->m_animation.get());
    [static_cast<CAKeyframeAnimation*>(m_animation.get()) setTimingFunctions:[other timingFunctions]];
}
