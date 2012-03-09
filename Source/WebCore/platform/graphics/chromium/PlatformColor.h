/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef PlatformColor_h
#define PlatformColor_h

#include "Extensions3D.h"
#include "GraphicsContext3D.h"
#include "SkTypes.h" 

namespace WebCore {

class PlatformColor {
public:
    static GraphicsContext3D::SourceDataFormat format()
    {
        return SK_B32_SHIFT ? GraphicsContext3D::SourceFormatRGBA8 : GraphicsContext3D::SourceFormatBGRA8;
    }

    // Returns the most efficient texture format for this platform.
    static GC3Denum bestTextureFormat(GraphicsContext3D* context)
    {
        GC3Denum textureFormat = GraphicsContext3D::RGBA;
        switch (format()) {
        case GraphicsContext3D::SourceFormatRGBA8:
            break;
        case GraphicsContext3D::SourceFormatBGRA8:
            if (context->getExtensions()->supports("GL_EXT_texture_format_BGRA8888"))
                textureFormat = Extensions3D::BGRA_EXT;
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        return textureFormat;
    }

    // Return true if the given texture format has the same component order
    // as the color on this platform.
    static bool sameComponentOrder(GC3Denum textureFormat)
    {
        switch (format()) {
        case GraphicsContext3D::SourceFormatRGBA8:
            return textureFormat == GraphicsContext3D::RGBA;
        case GraphicsContext3D::SourceFormatBGRA8:
            return textureFormat == Extensions3D::BGRA_EXT;
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    }
};

} // namespace WebCore

#endif
