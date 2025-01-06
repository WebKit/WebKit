/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "GenericMediaQueryTypes.h"

namespace WebCore {

class CSSToLengthConversionData;
class RenderBox;
class RenderStyle;
class RenderView;

namespace Calculation {
enum class Category : uint8_t;
}

namespace CQ {

// Interface exposed by schemas that can provide a value for the container-progress() function.
struct ContainerProgressProviding {
    virtual ~ContainerProgressProviding();

    virtual AtomString name() const = 0;
    virtual WebCore::Calculation::Category category() const = 0;
    virtual void collectComputedStyleDependencies(ComputedStyleDependencies&) const = 0;

    virtual double valueInCanonicalUnits(const RenderBox&) const = 0;
    virtual double valueInCanonicalUnits(const RenderView&, const RenderStyle&) const = 0;
};

namespace Features {

const MQ::FeatureSchema& width();
const MQ::FeatureSchema& height();
const MQ::FeatureSchema& inlineSize();
const MQ::FeatureSchema& blockSize();
const MQ::FeatureSchema& aspectRatio();
const MQ::FeatureSchema& orientation();
const MQ::FeatureSchema& style();

Vector<const MQ::FeatureSchema*> allSchemas();
Vector<const ContainerProgressProviding*> allContainerProgressProvidingSchemas();

} // namespace Features
} // namespace CQ
} // namespace WebCore
