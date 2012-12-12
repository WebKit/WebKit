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

#ifndef TextureMapperShaderManager_h
#define TextureMapperShaderManager_h

#if USE(TEXTURE_MAPPER)
#include "GraphicsContext3D.h"
#include "TextureMapperGL.h"
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/AtomicStringHash.h>

namespace WebCore {
#define TEXMAP_DECLARE_VARIABLE(Accessor, Name, Type) GC3Duint Accessor##Location() { static const AtomicString name(Name); return getLocation(name, Type); }
#define TEXMAP_DECLARE_UNIFORM(Accessor) TEXMAP_DECLARE_VARIABLE(Accessor, "u_"#Accessor, UniformVariable)
#define TEXMAP_DECLARE_ATTRIBUTE(Accessor) TEXMAP_DECLARE_VARIABLE(Accessor, "a_"#Accessor, AttribVariable)
#define TEXMAP_DECLARE_SAMPLER(Accessor) TEXMAP_DECLARE_VARIABLE(Accessor, "s_"#Accessor, UniformVariable)

class TextureMapperShaderProgram : public RefCounted<TextureMapperShaderProgram> {
public:
    Platform3DObject programID() const { return m_id; }
    GraphicsContext3D* context() { return m_context.get(); }
    static PassRefPtr<TextureMapperShaderProgram> create(PassRefPtr<GraphicsContext3D> context, const String& vertex, const String& fragment)
    {
        return adoptRef(new TextureMapperShaderProgram(context, vertex, fragment));
    }

    virtual ~TextureMapperShaderProgram();

    TEXMAP_DECLARE_ATTRIBUTE(vertex)
    TEXMAP_DECLARE_ATTRIBUTE(texCoord)

    TEXMAP_DECLARE_UNIFORM(matrix)
    TEXMAP_DECLARE_UNIFORM(flip)
    TEXMAP_DECLARE_UNIFORM(samplerSize)
    TEXMAP_DECLARE_UNIFORM(opacity)
    TEXMAP_DECLARE_UNIFORM(color)
    TEXMAP_DECLARE_UNIFORM(expandedQuadEdgesInScreenSpace)
    TEXMAP_DECLARE_SAMPLER(sampler)
    TEXMAP_DECLARE_SAMPLER(mask)

#if ENABLE(CSS_FILTERS)
    TEXMAP_DECLARE_UNIFORM(amount)
    TEXMAP_DECLARE_UNIFORM(gaussianKernel)
    TEXMAP_DECLARE_UNIFORM(blurRadius)
    TEXMAP_DECLARE_UNIFORM(shadowColor)
    TEXMAP_DECLARE_UNIFORM(shadowOffset)
    TEXMAP_DECLARE_SAMPLER(contentTexture)
#endif

private:
    TextureMapperShaderProgram(PassRefPtr<GraphicsContext3D>, const String& vertexShaderSource, const String& fragmentShaderSource);
    Platform3DObject m_vertexShader;
    Platform3DObject m_fragmentShader;

    enum VariableType { UniformVariable, AttribVariable };
    GC3Duint getLocation(const AtomicString&, VariableType);

    RefPtr<GraphicsContext3D> m_context;
    Platform3DObject m_id;
    HashMap<AtomicString, GC3Duint> m_variables;
};

class TextureMapperShaderManager {
public:
    enum ShaderKey {
        Invalid = 0,
        Default,
        Rect,
        Masked,
        MaskedRect,
        SolidColor,
        Antialiased,
        GrayscaleFilter,
        SepiaFilter,
        SaturateFilter,
        HueRotateFilter,
        BrightnessFilter,
        ContrastFilter,
        OpacityFilter,
        InvertFilter,
        BlurFilter,
        ShadowFilterPass1,
        ShadowFilterPass2,
        LastFilter
    };

    TextureMapperShaderManager() { }
    explicit TextureMapperShaderManager(GraphicsContext3D*);
    virtual ~TextureMapperShaderManager();

    PassRefPtr<TextureMapperShaderProgram> getShaderProgram(ShaderKey);

private:
    typedef HashMap<ShaderKey, RefPtr<TextureMapperShaderProgram>, DefaultHash<int>::Hash, HashTraits<int> > TextureMapperShaderProgramMap;
    TextureMapperShaderProgramMap m_programs;
    RefPtr<GraphicsContext3D> m_context;
};

}
#endif

#endif // TextureMapperShaderManager_h
