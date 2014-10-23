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
#import "PlatformCAAnimationRemote.h"

#import "ArgumentCoders.h"
#import "RemoteLayerTreeHost.h"
#import "WebCoreArgumentCoders.h"
#import <WebCore/BlockExceptions.h>
#import <WebCore/GraphicsLayer.h>
#import <WebCore/PlatformCAAnimationMac.h>
#import <WebCore/PlatformCAFilters.h>
#import <WebCore/TimingFunction.h>
#import <wtf/CurrentTime.h>
#import <wtf/RetainPtr.h>
#import <QuartzCore/QuartzCore.h>

using namespace WTF;
using namespace WebCore;

static double mediaTimeToCurrentTime(CFTimeInterval t)
{
    return monotonicallyIncreasingTime() + t - CACurrentMediaTime();
}

static NSString * const WKExplicitBeginTimeFlag = @"WKPlatformCAAnimationExplicitBeginTimeFlag";

@interface WKAnimationDelegate : NSObject {
    GraphicsLayer::PlatformLayerID _layerID;
    WebKit::RemoteLayerTreeHost* _layerTreeHost;
}

- (instancetype)initWithLayerID:(GraphicsLayer::PlatformLayerID)layerID layerTreeHost:(WebKit::RemoteLayerTreeHost*)layerTreeHost;
- (void)invalidate;
@end

@implementation WKAnimationDelegate

- (instancetype)initWithLayerID:(GraphicsLayer::PlatformLayerID)layerID layerTreeHost:(WebKit::RemoteLayerTreeHost*)layerTreeHost
{
    if ((self = [super init])) {
        _layerID = layerID;
        _layerTreeHost = layerTreeHost;
    }
    return self;
}

- (void)invalidate
{
    _layerTreeHost = nullptr;
}

- (void)animationDidStart:(CAAnimation *)animation
{
    if (!_layerTreeHost)
        return;

    bool hasExplicitBeginTime = [[animation valueForKey:WKExplicitBeginTimeFlag] boolValue];
    CFTimeInterval startTime;

    if (hasExplicitBeginTime) {
        // We don't know what time CA used to commit the animation, so just use the current time
        // (even though this will be slightly off).
        startTime = mediaTimeToCurrentTime(CACurrentMediaTime());
    } else
        startTime = mediaTimeToCurrentTime([animation beginTime]);

    _layerTreeHost->animationDidStart(_layerID, animation, startTime);
}
@end

namespace WebKit {

void PlatformCAAnimationRemote::KeyframeValue::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder.encodeEnum(keyType);

    switch (keyType) {
    case NumberKeyType:
        encoder << number;
        break;
    case ColorKeyType:
        encoder << color;
        break;
    case PointKeyType:
        encoder << point;
        break;
    case TransformKeyType:
        encoder << transform;
        break;
    case FilterKeyType:
        encoder << *filter.get();
        break;
    }
}

bool PlatformCAAnimationRemote::KeyframeValue::decode(IPC::ArgumentDecoder& decoder, PlatformCAAnimationRemote::KeyframeValue& value)
{
    if (!decoder.decodeEnum(value.keyType))
        return false;

    switch (value.keyType) {
    case NumberKeyType:
        if (!decoder.decode(value.number))
            return false;
        break;
    case ColorKeyType:
        if (!decoder.decode(value.color))
            return false;
        break;
    case PointKeyType:
        if (!decoder.decode(value.point))
            return false;
        break;
    case TransformKeyType:
        if (!decoder.decode(value.transform))
            return false;
        break;
    case FilterKeyType:
        if (!decodeFilterOperation(decoder, value.filter))
            return false;
        break;
    }

    return true;
}

void PlatformCAAnimationRemote::Properties::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << keyPath;
    encoder.encodeEnum(animationType);

    encoder << beginTime;
    encoder << duration;
    encoder << timeOffset;
    encoder << repeatCount;
    encoder << speed;

    encoder.encodeEnum(fillMode);
    encoder.encodeEnum(valueFunction);

    encoder << autoReverses;
    encoder << removedOnCompletion;
    encoder << additive;
    encoder << reverseTimingFunctions;
    encoder << hasExplicitBeginTime;
    
    encoder << keyValues;
    encoder << keyTimes;
    
    encoder << static_cast<uint64_t>(timingFunctions.size());
    for (const auto& timingFunction : timingFunctions) {
        switch (timingFunction->type()) {
        case TimingFunction::LinearFunction:
            encoder << *static_cast<LinearTimingFunction*>(timingFunction.get());
            break;
            
        case TimingFunction::CubicBezierFunction:
            encoder << *static_cast<CubicBezierTimingFunction*>(timingFunction.get());
            break;
        
        case TimingFunction::StepsFunction:
            encoder << *static_cast<StepsTimingFunction*>(timingFunction.get());
            break;
        }
    }
}

bool PlatformCAAnimationRemote::Properties::decode(IPC::ArgumentDecoder& decoder, PlatformCAAnimationRemote::Properties& properties)
{
    if (!decoder.decode(properties.keyPath))
        return false;

    if (!decoder.decodeEnum(properties.animationType))
        return false;

    if (!decoder.decode(properties.beginTime))
        return false;

    if (!decoder.decode(properties.duration))
        return false;

    if (!decoder.decode(properties.timeOffset))
        return false;

    if (!decoder.decode(properties.repeatCount))
        return false;

    if (!decoder.decode(properties.speed))
        return false;

    if (!decoder.decodeEnum(properties.fillMode))
        return false;

    if (!decoder.decodeEnum(properties.valueFunction))
        return false;

    if (!decoder.decode(properties.autoReverses))
        return false;

    if (!decoder.decode(properties.removedOnCompletion))
        return false;

    if (!decoder.decode(properties.additive))
        return false;

    if (!decoder.decode(properties.reverseTimingFunctions))
        return false;

    if (!decoder.decode(properties.hasExplicitBeginTime))
        return false;

    if (!decoder.decode(properties.keyValues))
        return false;

    if (!decoder.decode(properties.keyTimes))
        return false;

    uint64_t numTimingFunctions;
    if (!decoder.decode(numTimingFunctions))
        return false;
    
    if (numTimingFunctions) {
        properties.timingFunctions.reserveInitialCapacity(numTimingFunctions);

        for (size_t i = 0; i < numTimingFunctions; ++i) {
        
            TimingFunction::TimingFunctionType type;
            if (!decoder.decodeEnum(type))
                return false;

            RefPtr<TimingFunction> timingFunction;
            switch (type) {
            case TimingFunction::LinearFunction:
                timingFunction = LinearTimingFunction::create();
                if (!decoder.decode(*static_cast<LinearTimingFunction*>(timingFunction.get())))
                    return false;
                break;
                
            case TimingFunction::CubicBezierFunction:
                timingFunction = CubicBezierTimingFunction::create();
                if (!decoder.decode(*static_cast<CubicBezierTimingFunction*>(timingFunction.get())))
                    return false;
                break;
            
            case TimingFunction::StepsFunction:
                timingFunction = StepsTimingFunction::create();
                if (!decoder.decode(*static_cast<StepsTimingFunction*>(timingFunction.get())))
                    return false;
                break;
            }
            
            properties.timingFunctions.uncheckedAppend(timingFunction.release());
        }
    }

    return true;
}
    
PassRefPtr<PlatformCAAnimation> PlatformCAAnimationRemote::create(PlatformCAAnimation::AnimationType type, const String& keyPath)
{
    return adoptRef(new PlatformCAAnimationRemote(type, keyPath));
}

PassRefPtr<PlatformCAAnimation> PlatformCAAnimationRemote::copy() const
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

    toPlatformCAAnimationRemote(animation.get())->setHasExplicitBeginTime(hasExplicitBeginTime());
    
    // Copy the specific Basic or Keyframe values.
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

PlatformCAAnimationRemote::PlatformCAAnimationRemote(AnimationType type, const String& keyPath)
    : PlatformCAAnimation(type)
{
    m_properties.keyPath = keyPath;
    m_properties.animationType = type;
}

String PlatformCAAnimationRemote::keyPath() const
{
    return m_properties.keyPath;
}

CFTimeInterval PlatformCAAnimationRemote::beginTime() const
{
    return m_properties.beginTime;
}

void PlatformCAAnimationRemote::setBeginTime(CFTimeInterval value)
{
    m_properties.beginTime = value;
    
    // Also set a flag to tell us if we've passed in a 0 value. 
    // The flag is needed because later beginTime will get changed
    // to the time at which it fired and we need to know whether
    // or not it was 0 to begin with.
    if (value)
        m_properties.hasExplicitBeginTime = value;
}

CFTimeInterval PlatformCAAnimationRemote::duration() const
{
    return m_properties.duration;
}

void PlatformCAAnimationRemote::setDuration(CFTimeInterval value)
{
    m_properties.duration = value;
}

float PlatformCAAnimationRemote::speed() const
{
    return m_properties.speed;
}

void PlatformCAAnimationRemote::setSpeed(float value)
{
    m_properties.speed = value;
}

CFTimeInterval PlatformCAAnimationRemote::timeOffset() const
{
    return m_properties.timeOffset;
}

void PlatformCAAnimationRemote::setTimeOffset(CFTimeInterval value)
{
    m_properties.timeOffset = value;
}

float PlatformCAAnimationRemote::repeatCount() const
{
    return m_properties.repeatCount;
}

void PlatformCAAnimationRemote::setRepeatCount(float value)
{
    m_properties.repeatCount = value;
}

bool PlatformCAAnimationRemote::autoreverses() const
{
    return m_properties.autoReverses;
}

void PlatformCAAnimationRemote::setAutoreverses(bool value)
{
    m_properties.autoReverses = value;
}

PlatformCAAnimation::FillModeType PlatformCAAnimationRemote::fillMode() const
{
    return m_properties.fillMode;
}

void PlatformCAAnimationRemote::setFillMode(FillModeType value)
{
    m_properties.fillMode = value;
}

void PlatformCAAnimationRemote::setTimingFunction(const TimingFunction* value, bool reverse)
{
    Vector<RefPtr<TimingFunction>> timingFunctions;
    timingFunctions.append(value->clone());

    m_properties.timingFunctions = WTF::move(timingFunctions);
    m_properties.reverseTimingFunctions = reverse;
}

void PlatformCAAnimationRemote::copyTimingFunctionFrom(const PlatformCAAnimation* value)
{
    copyTimingFunctionsFrom(value);
}

bool PlatformCAAnimationRemote::isRemovedOnCompletion() const
{
    return m_properties.removedOnCompletion;
}

void PlatformCAAnimationRemote::setRemovedOnCompletion(bool value)
{
    m_properties.removedOnCompletion = value;
}

bool PlatformCAAnimationRemote::isAdditive() const
{
    return m_properties.additive;
}

void PlatformCAAnimationRemote::setAdditive(bool value)
{
    m_properties.additive = value;
}

PlatformCAAnimation::ValueFunctionType PlatformCAAnimationRemote::valueFunction() const
{
    return m_properties.valueFunction;
}

void PlatformCAAnimationRemote::setValueFunction(ValueFunctionType value)
{
    m_properties.valueFunction = value;
}

void PlatformCAAnimationRemote::setFromValue(float value)
{
    if (animationType() != Basic)
        return;

    m_properties.keyValues.resize(2);
    m_properties.keyValues[0] = KeyframeValue(value);
}

void PlatformCAAnimationRemote::setFromValue(const TransformationMatrix& value)
{
    if (animationType() != Basic)
        return;

    m_properties.keyValues.resize(2);
    m_properties.keyValues[0] = KeyframeValue(value);
}

void PlatformCAAnimationRemote::setFromValue(const FloatPoint3D& value)
{
    if (animationType() != Basic)
        return;

    m_properties.keyValues.resize(2);
    m_properties.keyValues[0] = KeyframeValue(value);
}

void PlatformCAAnimationRemote::setFromValue(const Color& value)
{
    if (animationType() != Basic)
        return;

    m_properties.keyValues.resize(2);
    m_properties.keyValues[0] = KeyframeValue(value);
}

#if ENABLE(CSS_FILTERS)
void PlatformCAAnimationRemote::setFromValue(const FilterOperation* operation, int internalFilterPropertyIndex)
{
    if (animationType() != Basic)
        return;

    m_properties.keyValues.resize(2);
    m_properties.keyValues[0] = KeyframeValue(operation->clone());
}
#endif

void PlatformCAAnimationRemote::copyFromValueFrom(const PlatformCAAnimation* value)
{
    const PlatformCAAnimationRemote* other = toPlatformCAAnimationRemote(value);

    if (other->m_properties.keyValues.isEmpty())
        return;
    
    m_properties.keyValues.resize(2);
    m_properties.keyValues[0] = other->m_properties.keyValues[0];
}

void PlatformCAAnimationRemote::setToValue(float value)
{
    if (animationType() != Basic)
        return;

    m_properties.keyValues.resize(2);
    m_properties.keyValues[1] = KeyframeValue(value);
}

void PlatformCAAnimationRemote::setToValue(const TransformationMatrix& value)
{
    if (animationType() != Basic)
        return;

    m_properties.keyValues.resize(2);
    m_properties.keyValues[1] = KeyframeValue(value);
}

void PlatformCAAnimationRemote::setToValue(const FloatPoint3D& value)
{
    if (animationType() != Basic)
        return;

    m_properties.keyValues.resize(2);
    m_properties.keyValues[1] = KeyframeValue(value);
}

void PlatformCAAnimationRemote::setToValue(const Color& value)
{
    if (animationType() != Basic)
        return;

    m_properties.keyValues.resize(2);
    m_properties.keyValues[1] = KeyframeValue(value);
}

#if ENABLE(CSS_FILTERS)
void PlatformCAAnimationRemote::setToValue(const FilterOperation* operation, int internalFilterPropertyIndex)
{
    if (animationType() != Basic)
        return;
    
    UNUSED_PARAM(internalFilterPropertyIndex);
    ASSERT(operation);
    m_properties.keyValues.resize(2);
    m_properties.keyValues[1] = KeyframeValue(operation->clone());
}
#endif

void PlatformCAAnimationRemote::copyToValueFrom(const PlatformCAAnimation* value)
{
    const PlatformCAAnimationRemote* other = toPlatformCAAnimationRemote(value);

    if (other->m_properties.keyValues.size() < 2)
        return;
    m_properties.keyValues.resize(2);
    m_properties.keyValues[1] = other->m_properties.keyValues[1];
}

// Keyframe-animation properties.
void PlatformCAAnimationRemote::setValues(const Vector<float>& values)
{
    if (animationType() != Keyframe)
        return;

    Vector<KeyframeValue> keyframes;
    keyframes.reserveInitialCapacity(values.size());
    
    for (size_t i = 0; i < values.size(); ++i)
        keyframes.uncheckedAppend(KeyframeValue(values[i]));
    
    m_properties.keyValues = WTF::move(keyframes);
}

void PlatformCAAnimationRemote::setValues(const Vector<TransformationMatrix>& values)
{
    if (animationType() != Keyframe)
        return;
        
    Vector<KeyframeValue> keyframes;
    keyframes.reserveInitialCapacity(values.size());
    
    for (size_t i = 0; i < values.size(); ++i)
        keyframes.uncheckedAppend(KeyframeValue(values[i]));
    
    m_properties.keyValues = WTF::move(keyframes);
}

void PlatformCAAnimationRemote::setValues(const Vector<FloatPoint3D>& values)
{
    if (animationType() != Keyframe)
        return;
        
    Vector<KeyframeValue> keyframes;
    keyframes.reserveInitialCapacity(values.size());
    
    for (size_t i = 0; i < values.size(); ++i)
        keyframes.uncheckedAppend(KeyframeValue(values[i]));
    
    m_properties.keyValues = WTF::move(keyframes);
}

void PlatformCAAnimationRemote::setValues(const Vector<Color>& values)
{
    if (animationType() != Keyframe)
        return;
        
    Vector<KeyframeValue> keyframes;
    keyframes.reserveInitialCapacity(values.size());
    
    for (size_t i = 0; i < values.size(); ++i)
        keyframes.uncheckedAppend(KeyframeValue(values[i]));
    
    m_properties.keyValues = WTF::move(keyframes);
}

#if ENABLE(CSS_FILTERS)
void PlatformCAAnimationRemote::setValues(const Vector<RefPtr<FilterOperation>>& values, int internalFilterPropertyIndex)
{
    UNUSED_PARAM(internalFilterPropertyIndex);
    
    if (animationType() != Keyframe)
        return;
        
    Vector<KeyframeValue> keyframes;
    keyframes.reserveInitialCapacity(values.size());
    
    for (size_t i = 0; i < values.size(); ++i)
        keyframes.uncheckedAppend(KeyframeValue(values[i]));
    
    m_properties.keyValues = WTF::move(keyframes);
}
#endif

void PlatformCAAnimationRemote::copyValuesFrom(const PlatformCAAnimation* value)
{
    const PlatformCAAnimationRemote* other = toPlatformCAAnimationRemote(value);
    m_properties.keyValues = other->m_properties.keyValues;
}

void PlatformCAAnimationRemote::setKeyTimes(const Vector<float>& keyTimes)
{
    m_properties.keyTimes = keyTimes;
}

void PlatformCAAnimationRemote::copyKeyTimesFrom(const PlatformCAAnimation* value)
{
    const PlatformCAAnimationRemote* other = toPlatformCAAnimationRemote(value);
    m_properties.keyTimes = other->m_properties.keyTimes;
}

void PlatformCAAnimationRemote::setTimingFunctions(const Vector<const TimingFunction*>& values, bool reverse)
{
    Vector<RefPtr<WebCore::TimingFunction>> timingFunctions;
    timingFunctions.reserveInitialCapacity(values.size());
    
    for (size_t i = 0; i < values.size(); ++i)
        timingFunctions.uncheckedAppend(values[i]->clone());
    
    m_properties.timingFunctions = WTF::move(timingFunctions);
    m_properties.reverseTimingFunctions = reverse;
}

void PlatformCAAnimationRemote::copyTimingFunctionsFrom(const PlatformCAAnimation* value)
{
    const PlatformCAAnimationRemote* other = toPlatformCAAnimationRemote(value);

    m_properties.timingFunctions = other->m_properties.timingFunctions;
    m_properties.reverseTimingFunctions = other->m_properties.reverseTimingFunctions;
}

static NSObject* animationValueFromKeyframeValue(const PlatformCAAnimationRemote::KeyframeValue& keyframeValue)
{
    switch (keyframeValue.keyframeType()) {
    case PlatformCAAnimationRemote::KeyframeValue::NumberKeyType:
        return @(keyframeValue.numberValue());
            
    case PlatformCAAnimationRemote::KeyframeValue::ColorKeyType: {
        Color color = keyframeValue.colorValue();
        return @[ @(color.red()), @(color.green()), @(color.blue()), @(color.alpha()) ];
    }

    case PlatformCAAnimationRemote::KeyframeValue::PointKeyType: {
        FloatPoint3D point = keyframeValue.pointValue();
        return @[ @(point.x()), @(point.y()), @(point.z()) ];
    }
    case PlatformCAAnimationRemote::KeyframeValue::TransformKeyType:
        return [NSValue valueWithCATransform3D:keyframeValue.transformValue()];
            
    case PlatformCAAnimationRemote::KeyframeValue::FilterKeyType:
        return PlatformCAFilters::filterValueForOperation(keyframeValue.filterValue(), 0 /* unused */).autorelease();
    }
}

static void addAnimationToLayer(CALayer *layer, RemoteLayerTreeHost* layerTreeHost, const String& key, const PlatformCAAnimationRemote::Properties& properties)
{
    RetainPtr<CAPropertyAnimation> caAnimation;
    switch (properties.animationType) {
    case PlatformCAAnimation::Basic: {
        RetainPtr<CABasicAnimation> basicAnimation;
        basicAnimation = [CABasicAnimation animationWithKeyPath:properties.keyPath];
        
        if (properties.keyValues.size() > 1) {
            [basicAnimation setFromValue:animationValueFromKeyframeValue(properties.keyValues[0])];
            [basicAnimation setToValue:animationValueFromKeyframeValue(properties.keyValues[1])];
        }
        
        if (properties.timingFunctions.size())
            [basicAnimation setTimingFunction:toCAMediaTimingFunction(properties.timingFunctions[0].get(), properties.reverseTimingFunctions)];
        
        caAnimation = basicAnimation;
        break;
    }
    case PlatformCAAnimation::Keyframe: {
        RetainPtr<CAKeyframeAnimation> keyframeAnimation;
        keyframeAnimation = [CAKeyframeAnimation animationWithKeyPath:properties.keyPath];

        if (properties.keyValues.size()) {
            RetainPtr<NSMutableArray> keyframes = adoptNS([[NSMutableArray alloc] initWithCapacity:properties.keyValues.size()]);
            for (const auto& value : properties.keyValues)
                [keyframes addObject:animationValueFromKeyframeValue(value)];
            
            [keyframeAnimation setValues:keyframes.get()];
        }

        if (properties.keyTimes.size()) {
            RetainPtr<NSMutableArray> keyTimes = adoptNS([[NSMutableArray alloc] initWithCapacity:properties.keyTimes.size()]);
            for (auto keyTime : properties.keyTimes)
                [keyTimes addObject:@(keyTime)];
            
            [keyframeAnimation setKeyTimes:keyTimes.get()];
        }

        if (properties.timingFunctions.size()) {
            RetainPtr<NSMutableArray> timingFunctions = adoptNS([[NSMutableArray alloc] initWithCapacity:properties.timingFunctions.size()]);
            for (const auto& timingFunction : properties.timingFunctions)
                [timingFunctions addObject:toCAMediaTimingFunction(timingFunction.get(), properties.reverseTimingFunctions)];
            
            [keyframeAnimation setTimingFunctions:timingFunctions.get()];
        }
        
        caAnimation = keyframeAnimation;
        break;
    }
    }
    
    [caAnimation setBeginTime:properties.beginTime];
    [caAnimation setDuration:properties.duration];
    [caAnimation setTimeOffset:properties.timeOffset];
    [caAnimation setRepeatCount:properties.repeatCount];
    [caAnimation setSpeed:properties.speed];
    [caAnimation setAutoreverses:properties.autoReverses];
    [caAnimation setRemovedOnCompletion:properties.removedOnCompletion];
    [caAnimation setAdditive:properties.additive];
    
    if (properties.fillMode != PlatformCAAnimation::NoFillMode)
        [caAnimation setFillMode:toCAFillModeType(properties.fillMode)];

    if (properties.valueFunction != PlatformCAAnimation::NoValueFunction)
        [caAnimation setValueFunction:[CAValueFunction functionWithName:toCAValueFunctionType(properties.valueFunction)]];
    
    if (properties.hasExplicitBeginTime)
        [caAnimation setValue:@YES forKey:WKExplicitBeginTimeFlag];
    
    if (layerTreeHost) {
        GraphicsLayer::PlatformLayerID layerID = RemoteLayerTreeHost::layerID(layer);
    
        RetainPtr<WKAnimationDelegate>& delegate = layerTreeHost->animationDelegates().add(layerID, nullptr).iterator->value;
        if (!delegate)
            delegate = adoptNS([[WKAnimationDelegate alloc] initWithLayerID:layerID layerTreeHost:layerTreeHost]);

        [caAnimation setDelegate:delegate.get()];
    }
    
    [layer addAnimation:caAnimation.get() forKey:key];
}

void PlatformCAAnimationRemote::updateLayerAnimations(CALayer *layer, RemoteLayerTreeHost* layerTreeHost, const AnimationsList& animationsToAdd, const HashSet<String>& animationsToRemove)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    for (const auto& value : animationsToRemove)
        [layer removeAnimationForKey:value];

    for (const auto& keyAnimationPair : animationsToAdd)
        addAnimationToLayer(layer, layerTreeHost, keyAnimationPair.first, keyAnimationPair.second);

    END_BLOCK_OBJC_EXCEPTIONS;
}

} // namespace WebKit
