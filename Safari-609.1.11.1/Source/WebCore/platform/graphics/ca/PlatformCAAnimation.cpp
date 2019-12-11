/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "PlatformCAAnimation.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, PlatformCAAnimation::AnimationType type)
{
    switch (type) {
    case PlatformCAAnimation::Basic: ts << "basic"; break;
    case PlatformCAAnimation::Keyframe: ts << "keyframe"; break;
    case PlatformCAAnimation::Spring: ts << "spring"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, PlatformCAAnimation::FillModeType fillMode)
{
    switch (fillMode) {
    case PlatformCAAnimation::NoFillMode: ts << "none"; break;
    case PlatformCAAnimation::Forwards: ts << "forwards"; break;
    case PlatformCAAnimation::Backwards: ts << "backwards"; break;
    case PlatformCAAnimation::Both: ts << "both"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, PlatformCAAnimation::ValueFunctionType valueFunctionType)
{
    switch (valueFunctionType) {
    case PlatformCAAnimation::NoValueFunction: ts << "none"; break;
    case PlatformCAAnimation::RotateX: ts << "rotateX"; break;
    case PlatformCAAnimation::RotateY: ts << "rotateY"; break;
    case PlatformCAAnimation::RotateZ: ts << "rotateX"; break;
    case PlatformCAAnimation::ScaleX: ts << "scaleX"; break;
    case PlatformCAAnimation::ScaleY: ts << "scaleY"; break;
    case PlatformCAAnimation::ScaleZ: ts << "scaleX"; break;
    case PlatformCAAnimation::Scale: ts << "scale"; break;
    case PlatformCAAnimation::TranslateX: ts << "translateX"; break;
    case PlatformCAAnimation::TranslateY: ts << "translateY"; break;
    case PlatformCAAnimation::TranslateZ: ts << "translateZ"; break;
    case PlatformCAAnimation::Translate: ts << "translate"; break;
    }
    return ts;
}

bool PlatformCAAnimation::isBasicAnimation() const
{
    return animationType() == Basic || animationType() == Spring;
}

} // namespace WebCore
