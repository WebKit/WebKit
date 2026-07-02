/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(THREADED_ANIMATIONS)

#include <wtf/JSONValues.h>

namespace WebCore {
enum class AcceleratedEffectProperty : uint16_t;
enum class CompositeOperation : uint8_t;
struct AcceleratedEffectValues;
struct AnimationEffectTiming;
class TimingFunction;
class WebAnimationTime;
}

namespace WebKit {

String toStringForTesting(WebCore::AcceleratedEffectProperty);
Ref<JSON::Value> toJSONForTesting(std::optional<WebCore::WebAnimationTime>);
Ref<JSON::Value> toJSONForTesting(std::optional<WebCore::CompositeOperation>);
Ref<JSON::Value> toJSONForTesting(const RefPtr<WebCore::TimingFunction>&);
Ref<JSON::Object> toJSONForTesting(const WebCore::AcceleratedEffectValues&, const OptionSet<WebCore::AcceleratedEffectProperty>&);
Ref<JSON::Object> toJSONForTesting(const WebCore::AnimationEffectTiming&);

}

#endif // ENABLE(THREADED_ANIMATIONS)
