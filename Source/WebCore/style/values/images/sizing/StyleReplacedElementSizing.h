/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/ImageSizingContext.h>
#include <WebCore/RenderStyleConstants.h>

namespace WebCore::Style {

class ReplacedElementSizing final : public ObjectSizeNegotiation::ImageSizingContext {
public:
    ReplacedElementSizing(FloatSize usedSize, ObjectFit fit, float density)
        : m_usedSize(usedSize)
        , m_fit(fit)
        , m_density(density)
    {
    }

    ConcreteObjectSize resolve(NaturalDimensions) const final;

private:
    ObjectSizeNegotiation::SpecifiedSize specifiedSize() const final { return ObjectSizeNegotiation::SpecifiedSize::none(); }
    FloatSize defaultObjectSize() const final { return m_usedSize; }
    float density() const final { return m_density; }

    FloatSize m_usedSize;
    ObjectFit m_fit;
    float m_density { 1 };
};

} // namespace WebCore::Style
