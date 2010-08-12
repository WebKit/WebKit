/*
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGImageBufferTools_h
#define SVGImageBufferTools_h

#if ENABLE(SVG)
#include "ImageBuffer.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class AffineTransform;
class FloatRect;
class GraphicsContext;
class RenderObject;

class SVGImageBufferTools : public Noncopyable {
public:
    static bool createImageBuffer(GraphicsContext*, const AffineTransform& absoluteTransform, const FloatRect& absoluteTargetRect, OwnPtr<ImageBuffer>&, ImageColorSpace);
    static void clipToImageBuffer(GraphicsContext*, const AffineTransform& absoluteTransform, const FloatRect& absoluteTargetRect, ImageBuffer*);

    static AffineTransform absoluteTransformFromContext(GraphicsContext*);

private:
    SVGImageBufferTools() { }
    ~SVGImageBufferTools() { }
};

}

#endif
#endif
