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

#if USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER)

#include "FloatQuad.h"
#include "GraphicsContext3D.h"
#include "IntSize.h"
#include "TextureMapperGL.h"
#include "TransformationMatrix.h"
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/CString.h>

#if ENABLE(CSS_FILTERS)
#include "FilterOperations.h"
#endif

namespace WebCore {

class BitmapTexture;
class TextureMapperShaderManager;

class TextureMapperShaderProgram : public RefCounted<TextureMapperShaderProgram> {
public:
    Platform3DObject id() { return m_id; }
    GC3Duint vertexAttrib() { return m_vertexAttrib; }

    TextureMapperShaderProgram(GraphicsContext3D*, const char* vertexShaderSource, const char* fragmentShaderSource);
    virtual ~TextureMapperShaderProgram();

    virtual void prepare(float opacity, const BitmapTexture*) { }
    GC3Dint matrixLocation() const { return m_matrixLocation; }
    GC3Dint flipLocation() const { return m_flipLocation; }
    GC3Dint textureSizeLocation() const { return m_textureSizeLocation; }
    GC3Dint sourceTextureLocation() const { return m_sourceTextureLocation; }
    GC3Dint maskTextureLocation() const { return m_maskTextureLocation; }
    GC3Dint opacityLocation() const { return m_opacityLocation; }

    static bool isValidUniformLocation(GC3Dint location) { return location >= 0; }

protected:
    void getUniformLocation(GC3Dint& var, const char* name);
    void initializeProgram();
    virtual void initialize() { }
    const char* vertexShaderSource() const { return m_vertexShaderSource.data(); }
    const char* fragmentShaderSource() const { return m_fragmentShaderSource.data(); }

    GraphicsContext3D* m_context;
    Platform3DObject m_id;
    GC3Duint m_vertexAttrib;
    Platform3DObject m_vertexShader;
    Platform3DObject m_fragmentShader;
    GC3Dint m_matrixLocation;
    GC3Dint m_flipLocation;
    GC3Dint m_textureSizeLocation;
    GC3Dint m_sourceTextureLocation;
    GC3Dint m_opacityLocation;
    GC3Dint m_maskTextureLocation;

private:
    CString m_vertexShaderSource;
    CString m_fragmentShaderSource;
};

#if ENABLE(CSS_FILTERS)
class StandardFilterProgram : public RefCounted<StandardFilterProgram> {
public:
    virtual ~StandardFilterProgram();
    virtual void prepare(const FilterOperation&, unsigned pass, const IntSize&, GC3Duint contentTexture);
    static PassRefPtr<StandardFilterProgram> create(GraphicsContext3D*, FilterOperation::OperationType, unsigned pass);
    GC3Duint vertexAttrib() const { return m_vertexAttrib; }
    GC3Duint texCoordAttrib() const { return m_texCoordAttrib; }
    GC3Duint textureUniform() const { return m_textureUniformLocation; }
protected:
    GraphicsContext3D* m_context;
private:
    StandardFilterProgram();
    StandardFilterProgram(GraphicsContext3D*, FilterOperation::OperationType, unsigned pass);
    Platform3DObject m_id;
    Platform3DObject m_vertexShader;
    Platform3DObject m_fragmentShader;
    GC3Duint m_vertexAttrib;
    GC3Duint m_texCoordAttrib;
    GC3Duint m_textureUniformLocation;
    union {
        GC3Duint amount;

        struct {
            GC3Duint radius;
            GC3Duint gaussianKernel;
        } blur;

        struct {
            GC3Duint blurRadius;
            GC3Duint color;
            GC3Duint offset;
            GC3Duint contentTexture;
            GC3Duint gaussianKernel;
        } shadow;
    } m_uniformLocations;
};
#endif

class TextureMapperShaderProgramSimple : public TextureMapperShaderProgram {
public:
    static PassRefPtr<TextureMapperShaderProgramSimple> create(GraphicsContext3D* context)
    {
        return adoptRef(new TextureMapperShaderProgramSimple(context));
    }

protected:
    TextureMapperShaderProgramSimple(GraphicsContext3D*);
private:
    TextureMapperShaderProgramSimple();
};

class TextureMapperShaderProgramRectSimple : public TextureMapperShaderProgram {
public:
    static PassRefPtr<TextureMapperShaderProgramRectSimple> create(GraphicsContext3D* context)
    {
        return adoptRef(new TextureMapperShaderProgramRectSimple(context));
    }

protected:
    TextureMapperShaderProgramRectSimple(GraphicsContext3D*);
private:
    TextureMapperShaderProgramRectSimple();
};

class TextureMapperShaderProgramOpacityAndMask : public TextureMapperShaderProgram {
public:
    static PassRefPtr<TextureMapperShaderProgramOpacityAndMask> create(GraphicsContext3D* context)
    {
        return adoptRef(new TextureMapperShaderProgramOpacityAndMask(context));
    }

protected:
    TextureMapperShaderProgramOpacityAndMask(GraphicsContext3D*);
private:
    TextureMapperShaderProgramOpacityAndMask();
};

class TextureMapperShaderProgramRectOpacityAndMask : public TextureMapperShaderProgram {
public:
    static PassRefPtr<TextureMapperShaderProgramRectOpacityAndMask> create(GraphicsContext3D* context)
    {
        return adoptRef(new TextureMapperShaderProgramRectOpacityAndMask(context));
    }

protected:
    TextureMapperShaderProgramRectOpacityAndMask(GraphicsContext3D*);
private:
    TextureMapperShaderProgramRectOpacityAndMask();
};

class TextureMapperShaderProgramSolidColor : public TextureMapperShaderProgram {
public:
    static PassRefPtr<TextureMapperShaderProgramSolidColor> create(GraphicsContext3D* context)
    {
        return adoptRef(new TextureMapperShaderProgramSolidColor(context));
    }

    GC3Dint colorLocation() const { return m_colorLocation; }

protected:
    TextureMapperShaderProgramSolidColor(GraphicsContext3D*);
private:
    TextureMapperShaderProgramSolidColor();
    GC3Dint m_colorLocation;
};

class TextureMapperShaderProgramAntialiasingNoMask : public TextureMapperShaderProgram {
public:
    static PassRefPtr<TextureMapperShaderProgramAntialiasingNoMask> create(GraphicsContext3D* context)
    {
        return adoptRef(new TextureMapperShaderProgramAntialiasingNoMask(context));
    }

    GC3Dint expandedQuadVerticesInTextureCoordinatesLocation() { return m_expandedQuadVerticesInTextureCordinatesLocation; }
    GC3Dint expandedQuadEdgesInScreenSpaceLocation() { return m_expandedQuadEdgesInScreenSpaceLocation; }

protected:
    TextureMapperShaderProgramAntialiasingNoMask(GraphicsContext3D*);
private:
    TextureMapperShaderProgramAntialiasingNoMask();

    GC3Dint m_expandedQuadVerticesInTextureCordinatesLocation;
    GC3Dint m_expandedQuadEdgesInScreenSpaceLocation;
};

class TextureMapperShaderManager {
public:
    enum ShaderType {
        Invalid = 0, // HashMaps do not like 0 as a key.
        Simple,
        AntialiasingNoMask,
        RectSimple,
        OpacityAndMask,
        RectOpacityAndMask,
        SolidColor
    };

    TextureMapperShaderManager() { }
    TextureMapperShaderManager(GraphicsContext3D*);
    virtual ~TextureMapperShaderManager();

#if ENABLE(CSS_FILTERS)
    unsigned getPassesRequiredForFilter(const FilterOperation&) const;
    PassRefPtr<StandardFilterProgram> getShaderForFilter(const FilterOperation&, unsigned pass);
#endif

    PassRefPtr<TextureMapperShaderProgram> getShaderProgram(ShaderType);
    PassRefPtr<TextureMapperShaderProgramSolidColor> solidColorProgram();
    PassRefPtr<TextureMapperShaderProgramAntialiasingNoMask> antialiasingNoMaskProgram();

private:
    typedef HashMap<ShaderType, RefPtr<TextureMapperShaderProgram>, DefaultHash<int>::Hash, HashTraits<int> > TextureMapperShaderProgramMap;
    TextureMapperShaderProgramMap m_textureMapperShaderProgramMap;
    GraphicsContext3D* m_context;

#if ENABLE(CSS_FILTERS)
    typedef HashMap<int, RefPtr<StandardFilterProgram> > FilterMap;
    FilterMap m_filterMap;
#endif
};

}

#endif

#endif // TextureMapperShaderManager_h
