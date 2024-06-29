/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/PlatformCAAnimation.h>
#include <wtf/Forward.h>

namespace WebKit {

struct PlatformCAAnimationRemoteProperties {
    String keyPath;
    WebCore::PlatformCAAnimation::AnimationType animationType { WebCore::PlatformCAAnimation::AnimationType::Basic };

    CFTimeInterval beginTime { 0 };
    double duration { 0 };
    double timeOffset { 0 };
    float repeatCount { 1 };
    float speed { 1 };

    WebCore::PlatformCAAnimation::FillModeType fillMode { WebCore::PlatformCAAnimation::FillModeType::NoFillMode };
    WebCore::PlatformCAAnimation::ValueFunctionType valueFunction { WebCore::PlatformCAAnimation::ValueFunctionType::NoValueFunction };
    RefPtr<WebCore::TimingFunction> timingFunction;

    bool autoReverses { false };
    bool removedOnCompletion { true };
    bool additive { false };
    bool reverseTimingFunctions { false };
    bool hasExplicitBeginTime { false };

    // For basic animations, these vectors have two entries. For keyframe animations, two or more.
    // timingFunctions has n-1 entries.
    using KeyframeValue = std::variant<float, WebCore::Color, WebCore::FloatPoint3D, WebCore::TransformationMatrix, Ref<WebCore::FilterOperation>>;
    Vector<KeyframeValue> keyValues;
    Vector<float> keyTimes;
    Vector<Ref<WebCore::TimingFunction>> timingFunctions;

    Vector<PlatformCAAnimationRemoteProperties> animations;
};

} // namespace WebKit
