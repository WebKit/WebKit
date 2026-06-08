/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPropertyNames.h"
#include "CompositeOperation.h"
#include "IterationCompositeOperation.h"
#include "WebAnimationTypes.h"
#include <wtf/HashSet.h>

namespace WebCore {

class Document;
class RenderStyle;
class Settings;

namespace Style::Interpolation {

class Client;

// MARK: - RenderStyle independent

// Returns whether the property can ever be interpolated.
//
// Controlled for non-custom properties by the "animation-type" key in CSSProperties.json.
bool canInterpolate(const AnimatableCSSProperty&);

// Returns whether the property supports additive or cumulative interpolation.
//
// Controlled for non-custom properties by the "animation-wrapper-requires-non-additive-or-cumulative-interpolation" key in CSSProperties.json.
bool isAdditiveOrCumulative(const AnimatableCSSProperty&);

// Returns whether the property supports accelerated animation.
//
// Controlled for non-custom properties by the "animation-wrapper-acceleration" key in CSSProperties.json.
bool isAccelerated(const AnimatableCSSProperty&, const Settings&);

// MARK: - RenderStyle dependent

// Returns whether the property's values in RenderStyle `a` and `b` are equal.
//
// Controlled for non-custom properties by implementing `equals` on the properties interpolation wrapper.
bool equals(const AnimatableCSSProperty&, const RenderStyle& a, const RenderStyle& b, const Document&);

// Returns whether the property can be interpolated with the values in RenderStyle `a` and `b`.
//
// Controlled for non-custom properties by implementing `canInterpolate` on the properties interpolation wrapper.
bool canInterpolate(const AnimatableCSSProperty&, const RenderStyle& a, const RenderStyle& b, const Document&);

// Interpolates the property from RenderStyle `a` to RenderStyle `b` into RenderStyle `destination`.
//
// Controlled for non-custom properties by implementing `interpolate` on the properties interpolation wrapper.
void interpolate(const AnimatableCSSProperty&, RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, double progress, CompositeOperation, IterationCompositeOperation, double currentIteration, const Client&);
void interpolate(const AnimatableCSSProperty&, RenderStyle& destination, const RenderStyle& from, const RenderStyle& to, double progress, CompositeOperation, const Client&);

// Returns whether the property requires interpolation for accumulative iteration for the values in RenderStyle `a` and `b`
//
// Controlled for non-custom properties by implementing `requiresInterpolationForAccumulativeIteration` on the properties interpolation wrapper.
bool requiresInterpolationForAccumulativeIteration(const AnimatableCSSProperty&, const RenderStyle& a, const RenderStyle& b, const Client&);

} // namespace Style::Interpolation
} // namespace WebCore
