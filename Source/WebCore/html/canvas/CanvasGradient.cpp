/*
 * Copyright (C) 2006-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
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
#include "CanvasGradient.h"

#include "CanvasStyle.h"
#include "Gradient.h"

namespace WebCore {

CanvasGradient::CanvasGradient(const FloatPoint& p0, const FloatPoint& p1)
    : m_gradient(Gradient::create(Gradient::LinearData { p0, p1 }, { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied }))
{
}

CanvasGradient::CanvasGradient(const FloatPoint& p0, float r0, const FloatPoint& p1, float r1)
    : m_gradient(Gradient::create(Gradient::RadialData { p0, p1, r0, r1, 1 }, { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied }))
{
}

CanvasGradient::CanvasGradient(const FloatPoint& centerPoint, float angleInRadians)
    : m_gradient(Gradient::create(Gradient::ConicData { centerPoint, angleInRadians }, { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied }))
{
}

Ref<CanvasGradient> CanvasGradient::create(const FloatPoint& p0, const FloatPoint& p1)
{
    return adoptRef(*new CanvasGradient(p0, p1));
}

Ref<CanvasGradient> CanvasGradient::create(const FloatPoint& p0, float r0, const FloatPoint& p1, float r1)
{
    return adoptRef(*new CanvasGradient(p0, r0, p1, r1));
}

Ref<CanvasGradient> CanvasGradient::create(const FloatPoint& centerPoint, float angleInRadians)
{
    return adoptRef(*new CanvasGradient(centerPoint, angleInRadians));
}

CanvasGradient::~CanvasGradient() = default;

ExceptionOr<void> CanvasGradient::addColorStop(ScriptExecutionContext& scriptExecutionContext, double value, const String& colorString)
{
    if (!(value >= 0 && value <= 1))
        return Exception { ExceptionCode::IndexSizeError };

    auto color = parseColor(colorString, scriptExecutionContext);
    if (!color.isValid())
        return Exception { ExceptionCode::SyntaxError };

    m_gradient->addColorStop({ static_cast<float>(value), WTFMove(color) });
    return { };
}

} // namespace WebCore
