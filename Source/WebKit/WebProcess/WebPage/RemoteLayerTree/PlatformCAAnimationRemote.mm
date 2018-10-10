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
#import "WKAnimationDelegate.h"
#import "WebCoreArgumentCoders.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/GraphicsLayer.h>
#import <WebCore/PlatformCAAnimationCocoa.h>
#import <WebCore/PlatformCAFilters.h>
#import <WebCore/TimingFunction.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/TextStream.h>

using namespace WTF;
using namespace WebCore;

static MonotonicTime mediaTimeToCurrentTime(CFTimeInterval t)
{
    return MonotonicTime::now() + Seconds(t - CACurrentMediaTime());
}

static NSString * const WKExplicitBeginTimeFlag = @"WKPlatformCAAnimationExplicitBeginTimeFlag";

@interface WKAnimationDelegate () <CAAnimationDelegate>
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
    MonotonicTime startTime;

    if (hasExplicitBeginTime) {
        // We don't know what time CA used to commit the animation, so just use the current time
        // (even though this will be slightly off).
        startTime = mediaTimeToCurrentTime(CACurrentMediaTime());
    } else
        startTime = mediaTimeToCurrentTime([animation beginTime]);

    _layerTreeHost->animationDidStart(_layerID, animation, startTime);
}

- (void)animationDidStop:(CAAnimation *)animation finished:(BOOL)finished
{
    if (!_layerTreeHost)
        return;

    _layerTreeHost->animationDidEnd(_layerID, animation);
}

@end

namespace WebKit {

void PlatformCAAnimationRemote::KeyframeValue::encode(IPC::Encoder& encoder) const
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

std::optional<PlatformCAAnimationRemote::KeyframeValue> PlatformCAAnimationRemote::KeyframeValue::decode(IPC::Decoder& decoder)
{
    PlatformCAAnimationRemote::KeyframeValue value;
    if (!decoder.decodeEnum(value.keyType))
        return std::nullopt;

    switch (value.keyType) {
    case NumberKeyType:
        if (!decoder.decode(value.number))
            return std::nullopt;
        break;
    case ColorKeyType:
        if (!decoder.decode(value.color))
            return std::nullopt;
        break;
    case PointKeyType:
        if (!decoder.decode(value.point))
            return std::nullopt;
        break;
    case TransformKeyType:
        if (!decoder.decode(value.transform))
            return std::nullopt;
        break;
    case FilterKeyType:
        if (!decodeFilterOperation(decoder, value.filter))
            return std::nullopt;
        break;
    }

    return WTFMove(value);
}

void PlatformCAAnimationRemote::Properties::encode(IPC::Encoder& encoder) const
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

        case TimingFunction::SpringFunction:
            encoder << *static_cast<SpringTimingFunction*>(timingFunction.get());
            break;
        }
    }
}

std::optional<PlatformCAAnimationRemote::Properties> PlatformCAAnimationRemote::Properties::decode(IPC::Decoder& decoder)
{
    PlatformCAAnimationRemote::Properties properties;
    if (!decoder.decode(properties.keyPath))
        return std::nullopt;

    if (!decoder.decodeEnum(properties.animationType))
        return std::nullopt;

    if (!decoder.decode(properties.beginTime))
        return std::nullopt;

    if (!decoder.decode(properties.duration))
        return std::nullopt;

    if (!decoder.decode(properties.timeOffset))
        return std::nullopt;

    if (!decoder.decode(properties.repeatCount))
        return std::nullopt;

    if (!decoder.decode(properties.speed))
        return std::nullopt;

    if (!decoder.decodeEnum(properties.fillMode))
        return std::nullopt;

    if (!decoder.decodeEnum(properties.valueFunction))
        return std::nullopt;

    if (!decoder.decode(properties.autoReverses))
        return std::nullopt;

    if (!decoder.decode(properties.removedOnCompletion))
        return std::nullopt;

    if (!decoder.decode(properties.additive))
        return std::nullopt;

    if (!decoder.decode(properties.reverseTimingFunctions))
        return std::nullopt;

    if (!decoder.decode(properties.hasExplicitBeginTime))
        return std::nullopt;

    if (!decoder.decode(properties.keyValues))
        return std::nullopt;

    if (!decoder.decode(properties.keyTimes))
        return std::nullopt;

    uint64_t numTimingFunctions;
    if (!decoder.decode(numTimingFunctions))
        return std::nullopt;
    
    if (numTimingFunctions) {
        properties.timingFunctions.reserveInitialCapacity(numTimingFunctions);

        for (size_t i = 0; i < numTimingFunctions; ++i) {
        
            TimingFunction::TimingFunctionType type;
            if (!decoder.decodeEnum(type))
                return std::nullopt;

            RefPtr<TimingFunction> timingFunction;
            switch (type) {
            case TimingFunction::LinearFunction:
                timingFunction = LinearTimingFunction::create();
                if (!decoder.decode(*static_cast<LinearTimingFunction*>(timingFunction.get())))
                    return std::nullopt;
                break;
                
            case TimingFunction::CubicBezierFunction:
                timingFunction = CubicBezierTimingFunction::create();
                if (!decoder.decode(*static_cast<CubicBezierTimingFunction*>(timingFunction.get())))
                    return std::nullopt;
                break;
            
            case TimingFunction::StepsFunction:
                timingFunction = StepsTimingFunction::create();
                if (!decoder.decode(*static_cast<StepsTimingFunction*>(timingFunction.get())))
                    return std::nullopt;
                break;

            case TimingFunction::SpringFunction:
                timingFunction = SpringTimingFunction::create();
                if (!decoder.decode(*static_cast<SpringTimingFunction*>(timingFunction.get())))
                    return std::nullopt;
                break;
            }
            
            properties.timingFunctions.uncheckedAppend(WTFMove(timingFunction));
        }
    }

    return WTFMove(properties);
}
    
Ref<PlatformCAAnimation> PlatformCAAnimationRemote::create(PlatformCAAnimation::AnimationType type, const String& keyPath)
{
    return adoptRef(*new PlatformCAAnimationRemote(type, keyPath));
}

Ref<PlatformCAAnimation> PlatformCAAnimationRemote::copy() const
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

    downcast<PlatformCAAnimationRemote>(animation.get()).setHasExplicitBeginTime(hasExplicitBeginTime());
    
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

    m_properties.timingFunctions = WTFMove(timingFunctions);
    m_properties.reverseTimingFunctions = reverse;
}

void PlatformCAAnimationRemote::copyTimingFunctionFrom(const PlatformCAAnimation& value)
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

void PlatformCAAnimationRemote::setFromValue(const FilterOperation* operation, int internalFilterPropertyIndex)
{
    if (animationType() != Basic)
        return;

    m_properties.keyValues.resize(2);
    m_properties.keyValues[0] = KeyframeValue(operation->clone());
}

void PlatformCAAnimationRemote::copyFromValueFrom(const PlatformCAAnimation& value)
{
    const PlatformCAAnimationRemote& other = downcast<PlatformCAAnimationRemote>(value);

    if (other.m_properties.keyValues.isEmpty())
        return;
    
    m_properties.keyValues.resize(2);
    m_properties.keyValues[0] = other.m_properties.keyValues[0];
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

void PlatformCAAnimationRemote::setToValue(const FilterOperation* operation, int internalFilterPropertyIndex)
{
    if (animationType() != Basic)
        return;
    
    UNUSED_PARAM(internalFilterPropertyIndex);
    ASSERT(operation);
    m_properties.keyValues.resize(2);
    m_properties.keyValues[1] = KeyframeValue(operation->clone());
}

void PlatformCAAnimationRemote::copyToValueFrom(const PlatformCAAnimation& value)
{
    const PlatformCAAnimationRemote& other = downcast<PlatformCAAnimationRemote>(value);

    if (other.m_properties.keyValues.size() < 2)
        return;
    m_properties.keyValues.resize(2);
    m_properties.keyValues[1] = other.m_properties.keyValues[1];
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
    
    m_properties.keyValues = WTFMove(keyframes);
}

void PlatformCAAnimationRemote::setValues(const Vector<TransformationMatrix>& values)
{
    if (animationType() != Keyframe)
        return;

    Vector<KeyframeValue> keyframes;
    keyframes.reserveInitialCapacity(values.size());
    
    for (size_t i = 0; i < values.size(); ++i)
        keyframes.uncheckedAppend(KeyframeValue(values[i]));
    
    m_properties.keyValues = WTFMove(keyframes);
}

void PlatformCAAnimationRemote::setValues(const Vector<FloatPoint3D>& values)
{
    if (animationType() != Keyframe)
        return;

    Vector<KeyframeValue> keyframes;
    keyframes.reserveInitialCapacity(values.size());
    
    for (size_t i = 0; i < values.size(); ++i)
        keyframes.uncheckedAppend(KeyframeValue(values[i]));
    
    m_properties.keyValues = WTFMove(keyframes);
}

void PlatformCAAnimationRemote::setValues(const Vector<Color>& values)
{
    if (animationType() != Keyframe)
        return;

    Vector<KeyframeValue> keyframes;
    keyframes.reserveInitialCapacity(values.size());
    
    for (size_t i = 0; i < values.size(); ++i)
        keyframes.uncheckedAppend(KeyframeValue(values[i]));
    
    m_properties.keyValues = WTFMove(keyframes);
}

void PlatformCAAnimationRemote::setValues(const Vector<RefPtr<FilterOperation>>& values, int internalFilterPropertyIndex)
{
    UNUSED_PARAM(internalFilterPropertyIndex);
    
    if (animationType() != Keyframe)
        return;

    Vector<KeyframeValue> keyframes;
    keyframes.reserveInitialCapacity(values.size());
    
    for (auto& value : values)
        keyframes.uncheckedAppend(KeyframeValue { value.copyRef() });
    
    m_properties.keyValues = WTFMove(keyframes);
}

void PlatformCAAnimationRemote::copyValuesFrom(const PlatformCAAnimation& value)
{
    m_properties.keyValues = downcast<PlatformCAAnimationRemote>(value).m_properties.keyValues;
}

void PlatformCAAnimationRemote::setKeyTimes(const Vector<float>& keyTimes)
{
    m_properties.keyTimes = keyTimes;
}

void PlatformCAAnimationRemote::copyKeyTimesFrom(const PlatformCAAnimation& value)
{
    m_properties.keyTimes = downcast<PlatformCAAnimationRemote>(value).m_properties.keyTimes;
}

void PlatformCAAnimationRemote::setTimingFunctions(const Vector<const TimingFunction*>& values, bool reverse)
{
    Vector<RefPtr<WebCore::TimingFunction>> timingFunctions;
    timingFunctions.reserveInitialCapacity(values.size());
    
    for (size_t i = 0; i < values.size(); ++i)
        timingFunctions.uncheckedAppend(values[i]->clone());
    
    m_properties.timingFunctions = WTFMove(timingFunctions);
    m_properties.reverseTimingFunctions = reverse;
}

void PlatformCAAnimationRemote::copyTimingFunctionsFrom(const PlatformCAAnimation& value)
{
    const PlatformCAAnimationRemote& other = downcast<PlatformCAAnimationRemote>(value);

    m_properties.timingFunctions = other.m_properties.timingFunctions;
    m_properties.reverseTimingFunctions = other.m_properties.reverseTimingFunctions;
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
    case PlatformCAAnimation::Spring: {
        RetainPtr<CASpringAnimation> springAnimation;
        springAnimation = [CASpringAnimation animationWithKeyPath:properties.keyPath];
        
        if (properties.keyValues.size() > 1) {
            [springAnimation setFromValue:animationValueFromKeyframeValue(properties.keyValues[0])];
            [springAnimation setToValue:animationValueFromKeyframeValue(properties.keyValues[1])];
        }
        
        if (properties.timingFunctions.size()) {
            auto& timingFunction = properties.timingFunctions[0];
            if (timingFunction->isSpringTimingFunction()) {
                auto& function = *static_cast<const SpringTimingFunction*>(timingFunction.get());
                [springAnimation setMass:function.mass()];
                [springAnimation setStiffness:function.stiffness()];
                [springAnimation setDamping:function.damping()];
                [springAnimation setInitialVelocity:function.initialVelocity()];
            }
        }
        caAnimation = springAnimation;
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

TextStream& operator<<(TextStream&ts, const PlatformCAAnimationRemote::KeyframeValue& value)
{
    switch (value.keyframeType()) {
    case PlatformCAAnimationRemote::KeyframeValue::NumberKeyType:
        ts << "number=" << value.numberValue();
        break;
    case PlatformCAAnimationRemote::KeyframeValue::ColorKeyType:
        ts << "color=";
        ts << value.colorValue();
        break;
    case PlatformCAAnimationRemote::KeyframeValue::PointKeyType:
        ts << "point=";
        ts << value.pointValue();
        break;
    case PlatformCAAnimationRemote::KeyframeValue::TransformKeyType:
        ts << "transform=";
        ts << value.transformValue();
        break;
    case PlatformCAAnimationRemote::KeyframeValue::FilterKeyType:
        ts << "filter=";
        if (value.filterValue())
            ts << *value.filterValue();
        else
            ts << "null";
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const PlatformCAAnimationRemote::Properties& animation)
{
    ts << "type=";
    ts << animation.animationType;
    ts << " keyPath=";
    ts << animation.keyPath;

    if (animation.beginTime)
        ts.dumpProperty("beginTime", animation.beginTime);

    if (animation.duration)
        ts.dumpProperty("duration", animation.duration);

    if (animation.timeOffset)
        ts.dumpProperty("timeOffset", animation.timeOffset);

    ts.dumpProperty("repeatCount", animation.repeatCount);

    if (animation.speed != 1)
        ts.dumpProperty("speed", animation.speed);

    ts.dumpProperty("fillMode", animation.fillMode);
    ts.dumpProperty("valueFunction", animation.valueFunction);

    if (animation.autoReverses)
        ts.dumpProperty("autoReverses", animation.autoReverses);

    if (!animation.removedOnCompletion)
        ts.dumpProperty("removedOnCompletion", animation.removedOnCompletion);

    if (animation.additive)
        ts.dumpProperty("additive", animation.additive);

    if (animation.reverseTimingFunctions)
        ts.dumpProperty("reverseTimingFunctions", animation.reverseTimingFunctions);

    if (animation.hasExplicitBeginTime)
        ts.dumpProperty("hasExplicitBeginTime", animation.hasExplicitBeginTime);

    ts << "\n";
    ts.increaseIndent();
    ts.writeIndent();
    ts << "(" << "keyframes";
    ts.increaseIndent();

    size_t maxFrames = std::max(animation.keyValues.size(), animation.keyTimes.size());
    maxFrames = std::max(maxFrames, animation.timingFunctions.size());

    for (size_t i = 0; i < maxFrames; ++i) {
        ts << "\n";
        ts.writeIndent();
        ts << "(keyframe " << unsigned(i);
        if (i < animation.keyTimes.size())
            ts.dumpProperty("time", animation.keyTimes[i]);

        if (i < animation.timingFunctions.size() && animation.timingFunctions[i])
            ts.dumpProperty<const TimingFunction&>("timing function", *animation.timingFunctions[i]);

        if (i < animation.keyValues.size())
            ts.dumpProperty("value", animation.keyValues[i]);

        ts << ")";
    }

    ts.decreaseIndent();
    ts.decreaseIndent();

    return ts;
}

} // namespace WebKit
