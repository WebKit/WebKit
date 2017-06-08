/*
 Copyright (C) 2016 Igalia S.L.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#ifndef VideoTextureCopierGStreamer_h
#define VideoTextureCopierGStreamer_h

#if USE(GSTREAMER_GL)

#include "GraphicsContext3D.h"
#include "TransformationMatrix.h"

namespace WebCore {

class TextureMapperShaderProgram;
class ImageOrientation;

class VideoTextureCopierGStreamer {
public:
    enum class ColorConversion {
        ConvertBGRAToRGBA,
        ConvertARGBToRGBA
    };

    VideoTextureCopierGStreamer(ColorConversion);
    ~VideoTextureCopierGStreamer();

    bool copyVideoTextureToPlatformTexture(Platform3DObject inputTexture, IntSize& frameSize, Platform3DObject outputTexture, GC3Denum outputTarget, GC3Dint level, GC3Denum internalFormat, GC3Denum format, GC3Denum type, bool flipY, ImageOrientation& sourceOrientation);
    void updateColorConversionMatrix(ColorConversion);
    void updateTextureSpaceMatrix();
    void updateTransformationMatrix();
    Platform3DObject resultTexture() { return m_resultTexture; }

private:
    RefPtr<GraphicsContext3D> m_context3D;
    RefPtr<TextureMapperShaderProgram> m_shaderProgram;
    Platform3DObject m_framebuffer { 0 };
    Platform3DObject m_vbo { 0 };
    bool m_flipY { false };
    ImageOrientation m_orientation;
    IntSize m_size;
    TransformationMatrix m_modelViewMatrix;
    TransformationMatrix m_projectionMatrix;
    TransformationMatrix m_textureSpaceMatrix;
    TransformationMatrix m_colorConversionMatrix;
    Platform3DObject m_resultTexture { 0 };
};

} // namespace WebCore

#endif // USE(GSTREAMER_GL)

#endif // VideoTextureCopierGStreamer_h
