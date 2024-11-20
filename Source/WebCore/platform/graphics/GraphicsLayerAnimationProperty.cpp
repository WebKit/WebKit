/*
 * Copyright (C) 2009-2023 Apple Inc. All rights reserved.
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
#include "GraphicsLayerAnimationProperty.h"

#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

String animatedPropertyIDAsString(GraphicsLayerAnimationProperty property)
{
    switch (property) {
    case GraphicsLayerAnimationProperty::Translate:
    case GraphicsLayerAnimationProperty::Scale:
    case GraphicsLayerAnimationProperty::Rotate:
    case GraphicsLayerAnimationProperty::Transform:
        return "transform"_s;
    case GraphicsLayerAnimationProperty::Opacity:
        return "opacity"_s;
    case GraphicsLayerAnimationProperty::BackgroundColor:
        return "background-color"_s;
    case GraphicsLayerAnimationProperty::Filter:
        return "filter"_s;
    case GraphicsLayerAnimationProperty::WebkitBackdropFilter:
        return "backdrop-filter"_s;
    case GraphicsLayerAnimationProperty::Invalid:
        return "invalid"_s;
    }
    ASSERT_NOT_REACHED();
    return ""_s;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, GraphicsLayerAnimationProperty property)
{
    switch (property) {
    case GraphicsLayerAnimationProperty::Invalid: ts << "invalid"; break;
    case GraphicsLayerAnimationProperty::Translate: ts << "translate"; break;
    case GraphicsLayerAnimationProperty::Scale: ts << "scale"; break;
    case GraphicsLayerAnimationProperty::Rotate: ts << "rotate"; break;
    case GraphicsLayerAnimationProperty::Transform: ts << "transform"; break;
    case GraphicsLayerAnimationProperty::Opacity: ts << "opacity"; break;
    case GraphicsLayerAnimationProperty::BackgroundColor: ts << "background-color"; break;
    case GraphicsLayerAnimationProperty::Filter: ts << "filter"; break;
    case GraphicsLayerAnimationProperty::WebkitBackdropFilter: ts << "backdrop-filter"; break;
    }
    return ts;
}

} // namespace WebCore
