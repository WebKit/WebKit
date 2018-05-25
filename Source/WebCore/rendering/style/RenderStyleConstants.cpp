/*
 * Copyright (C) 2015 Apple Inc.  All rights reserved.
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
#include "RenderStyleConstants.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, Visibility visibility)
{
    switch (visibility) {
    case Visibility::Visible: ts << "visible"; break;
    case Visibility::Hidden: ts << "hidden"; break;
    case Visibility::Collapse: ts << "collapse"; break;
    }
    
    return ts;
}

TextStream& operator<<(TextStream& ts, ImageRendering imageRendering)
{
    switch (imageRendering) {
    case ImageRendering::Auto: ts << "auto"; break;
    case ImageRendering::OptimizeSpeed: ts << "optimizeSpeed"; break;
    case ImageRendering::OptimizeQuality: ts << "optimizeQuality"; break;
    case ImageRendering::CrispEdges: ts << "crispEdges"; break;
    case ImageRendering::Pixelated: ts << "pixelated"; break;
    }
    
    return ts;
}

TextStream& operator<<(TextStream& ts, FillSizeType sizeType)
{
    switch (sizeType) {
    case FillSizeType::Contain: ts << "contain"; break;
    case FillSizeType::Cover: ts << "cover"; break;
    case FillSizeType::Size: ts << "size-length"; break;
    case FillSizeType::None: ts << "size-none"; break;
    }
    
    return ts;
}

TextStream& operator<<(TextStream& ts, FillAttachment attachment)
{
    switch (attachment) {
    case FillAttachment::ScrollBackground: ts << "scroll"; break;
    case FillAttachment::LocalBackground: ts << "local"; break;
    case FillAttachment::FixedBackground: ts << "fixed"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, FillBox fill)
{
    switch (fill) {
    case FillBox::Border: ts << "border"; break;
    case FillBox::Padding: ts << "padding"; break;
    case FillBox::Content: ts << "content"; break;
    case FillBox::Text: ts << "text"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, FillRepeat repeat)
{
    switch (repeat) {
    case FillRepeat::Repeat: ts << "repeat"; break;
    case FillRepeat::NoRepeat: ts << "no-repeat"; break;
    case FillRepeat::Round: ts << "round"; break;
    case FillRepeat::Space: ts << "space"; break;
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, MaskSourceType maskSource)
{
    switch (maskSource) {
    case MaskSourceType::Alpha: ts << "alpha"; break;
    case MaskSourceType::Luminance: ts << "luminance"; break;
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, Edge edge)
{
    switch (edge) {
    case Edge::Top: ts << "top"; break;
    case Edge::Right: ts << "right"; break;
    case Edge::Bottom: ts << "bottom"; break;
    case Edge::Left: ts << "left"; break;
    }
    return ts;
}

bool alwaysPageBreak(BreakBetween between)
{
    return between >= BreakBetween::Page;
}

const float defaultMiterLimit = 4;

} // namespace WebCore
