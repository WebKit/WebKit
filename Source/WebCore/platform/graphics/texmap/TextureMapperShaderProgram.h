/*
 Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 Copyright (C) 2012 Igalia S.L.

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

#ifndef TextureMapperShaderProgram_h
#define TextureMapperShaderProgram_h

#if USE(TEXTURE_MAPPER_GL)

#include "TextureMapperGLHeaders.h"
#include "TransformationMatrix.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

#define TEXMAP_ATTRIBUTE_VARIABLES(macro) \
    macro(vertex) \

#define TEXMAP_UNIFORM_VARIABLES(macro) \
    macro(modelViewMatrix) \
    macro(projectionMatrix) \
    macro(textureSpaceMatrix) \
    macro(textureColorSpaceMatrix) \
    macro(opacity) \
    macro(color) \
    macro(expandedQuadEdgesInScreenSpace) \
    macro(yuvToRgb) \
    macro(filterAmount) \
    macro(gaussianKernel) \
    macro(blurRadius) \
    macro(shadowOffset) \
    macro(roundedRectNumber) \
    macro(roundedRect) \
    macro(roundedRectInverseTransformMatrix) \

#define TEXMAP_SAMPLER_VARIABLES(macro) \
    macro(sampler) \
    macro(samplerY) \
    macro(samplerU) \
    macro(samplerV) \
    macro(mask) \
    macro(contentTexture) \
    macro(externalOESTexture) \

#define TEXMAP_VARIABLES(macro) \
    TEXMAP_ATTRIBUTE_VARIABLES(macro) \
    TEXMAP_UNIFORM_VARIABLES(macro) \
    TEXMAP_SAMPLER_VARIABLES(macro) \

#define TEXMAP_DECLARE_VARIABLE(Accessor, Name, Type) \
    GLuint Accessor##Location() { \
        return getLocation(VariableID::Accessor, Name, Type); \
    }

#define TEXMAP_DECLARE_UNIFORM(Accessor) TEXMAP_DECLARE_VARIABLE(Accessor, "u_"#Accessor""_s, UniformVariable)
#define TEXMAP_DECLARE_ATTRIBUTE(Accessor) TEXMAP_DECLARE_VARIABLE(Accessor, "a_"#Accessor""_s, AttribVariable)
#define TEXMAP_DECLARE_SAMPLER(Accessor) TEXMAP_DECLARE_VARIABLE(Accessor, "s_"#Accessor""_s, UniformVariable)

#define TEXMAP_DECLARE_VARIABLE_ENUM(name) name,

class TextureMapperShaderProgram : public RefCounted<TextureMapperShaderProgram> {
public:
    enum Option {
        TextureRGB       = 1L << 0,
        Rect             = 1L << 1,
        SolidColor       = 1L << 2,
        Opacity          = 1L << 3,
        Antialiasing     = 1L << 5,
        GrayscaleFilter  = 1L << 6,
        SepiaFilter      = 1L << 7,
        SaturateFilter   = 1L << 8,
        HueRotateFilter  = 1L << 9,
        BrightnessFilter = 1L << 10,
        ContrastFilter   = 1L << 11,
        InvertFilter     = 1L << 12,
        OpacityFilter    = 1L << 13,
        BlurFilter       = 1L << 14,
        AlphaBlur        = 1L << 15,
        ContentTexture   = 1L << 16,
        ManualRepeat     = 1L << 17,
        TextureYUV       = 1L << 18,
        TextureNV12      = 1L << 19,
        TextureNV21      = 1L << 20,
        TexturePackedYUV = 1L << 21,
        TextureExternalOES = 1L << 22,
        RoundedRectClip  = 1L << 23,
    };

    enum class VariableID {
        TEXMAP_VARIABLES(TEXMAP_DECLARE_VARIABLE_ENUM)
    };

    typedef unsigned Options;

    static Ref<TextureMapperShaderProgram> create(Options);
    virtual ~TextureMapperShaderProgram();

    GLuint programID() const { return m_id; }


    TEXMAP_ATTRIBUTE_VARIABLES(TEXMAP_DECLARE_ATTRIBUTE)
    TEXMAP_UNIFORM_VARIABLES(TEXMAP_DECLARE_UNIFORM)
    TEXMAP_SAMPLER_VARIABLES(TEXMAP_DECLARE_SAMPLER)

    void setMatrix(GLuint, const TransformationMatrix&);

private:
    TextureMapperShaderProgram(const String& vertexShaderSource, const String& fragmentShaderSource);

    GLuint m_vertexShader;
    GLuint m_fragmentShader;

    enum VariableType { UniformVariable, AttribVariable };
    GLuint getLocation(VariableID, ASCIILiteral, VariableType);

    GLuint m_id;
    HashMap<VariableID, GLuint, WTF::IntHash<VariableID>, WTF::StrongEnumHashTraits<VariableID>> m_variables;
};

}
#endif // USE(TEXTURE_MAPPER_GL)

#endif // TextureMapperShaderProgram_h
