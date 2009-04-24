/*
 * Copyright (C) 2004, 2005 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SVGRenderTreeAsText_h
#define SVGRenderTreeAsText_h

#if ENABLE(SVG)

#include "TextStream.h"

namespace WebCore {

    class TransformationMatrix;
    class Color;
    class FloatPoint;
    class FloatRect;
    class FloatSize;
    class IntPoint;
    class IntRect;
    class Node;
    class RenderPath;
    class RenderSVGContainer;
    class RenderSVGInlineText;
    class RenderSVGRoot;
    class RenderSVGText; 
    class RenderSVGImage;

// functions used by the main RenderTreeAsText code
void write(TextStream&, const RenderPath&, int indent = 0);
void write(TextStream&, const RenderSVGContainer&, int indent = 0);
void write(TextStream&, const RenderSVGInlineText&, int indent = 0);
void write(TextStream&, const RenderSVGRoot&, int indent = 0);
void write(TextStream&, const RenderSVGText&, int indent = 0);
void write(TextStream&, const RenderSVGImage&, int indent = 0);

void writeRenderResources(TextStream&, Node* parent);

// helper operators defined used in various classes to dump the render tree.
TextStream& operator<<(TextStream&, const TransformationMatrix&);
TextStream& operator<<(TextStream&, const IntRect&);
TextStream& operator<<(TextStream&, const Color&);
TextStream& operator<<(TextStream&, const IntPoint&);
TextStream& operator<<(TextStream&, const FloatSize&);
TextStream& operator<<(TextStream&, const FloatRect&);
TextStream& operator<<(TextStream&, const FloatPoint&);

// helper operators specific to dumping the render tree. these are used in various classes to dump the render tree
// these could be defined in separate namespace to avoid matching these generic signatures unintentionally.

template<typename Item>
TextStream& operator<<(TextStream& ts, const Vector<Item*>& v)
{
    ts << "[";

    for (unsigned i = 0; i < v.size(); i++) {
        ts << *v[i];
        if (i < v.size() - 1)
            ts << ", ";
    }

    ts << "]";
    return ts;
}

template<typename Item>
TextStream& operator<<(TextStream& ts, const Vector<Item>& v)
{
    ts << "[";

    for (unsigned i = 0; i < v.size(); i++) {
        ts << v[i];
        if (i < v.size() - 1)
            ts << ", ";
    }

    ts << "]";
    return ts;
}

template<typename Pointer>
TextStream& operator<<(TextStream& ts, Pointer* t)
{
    ts << reinterpret_cast<intptr_t>(t);
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG)

#endif // SVGRenderTreeAsText_h
