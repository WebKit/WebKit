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
#include "IntSize.h"
#include "OpenGLShims.h"
#include "TextureMapper.h"
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
    GLuint id() { return m_id; }
    GLuint vertexAttrib() { return m_vertexAttrib; }

    TextureMapperShaderProgram(const char* vertexShaderSource, const char* fragmentShaderSource);
    virtual ~TextureMapperShaderProgram();

    virtual void prepare(float opacity, const BitmapTexture*) { }
    GLint matrixLocation() const { return m_matrixLocation; }
    GLint flipLocation() const { return m_flipLocation; }
    GLint textureSizeLocation() const { return m_textureSizeLocation; }
    GLint sourceTextureLocation() const { return m_sourceTextureLocation; }
    GLint maskTextureLocation() const { return m_maskTextureLocation; }
    GLint opacityLocation() const { return m_opacityLocation; }

    static bool isValidUniformLocation(GLint location) { return location >= 0; }

protected:
    void getUniformLocation(GLint& var, const char* name);
    void initializeProgram();
    virtual void initialize() { }
    const char* vertexShaderSource() const { return m_vertexShaderSource.data(); }
    const char* fragmentShaderSource() const { return m_fragmentShaderSource.data(); }

    GLuint m_id;
    GLuint m_vertexAttrib;
    GLuint m_vertexShader;
    GLuint m_fragmentShader;
    GLint m_matrixLocation;
    GLint m_flipLocation;
    GLint m_textureSizeLocation;
    GLint m_sourceTextureLocation;
    GLint m_opacityLocation;
    GLint m_maskTextureLocation;

private:
    CString m_vertexShaderSource;
    CString m_fragmentShaderSource;
};

#if ENABLE(CSS_FILTERS)
class StandardFilterProgram : public RefCounted<StandardFilterProgram> {
public:
    virtual ~StandardFilterProgram();
    virtual void prepare(const FilterOperation&, unsigned pass, const IntSize&, GLuint contentTexture);
    static PassRefPtr<StandardFilterProgram> create(FilterOperation::OperationType, unsigned pass);
    GLuint vertexAttrib() const { return m_vertexAttrib; }
    GLuint texCoordAttrib() const { return m_texCoordAttrib; }
    GLuint textureUniform() const { return m_textureUniformLocation; }
private:
    StandardFilterProgram(FilterOperation::OperationType, unsigned pass);
    GLuint m_id;
    GLuint m_vertexShader;
    GLuint m_fragmentShader;
    GLuint m_vertexAttrib;
    GLuint m_texCoordAttrib;
    GLuint m_textureUniformLocation;
    union {
        GLuint amount;

        struct {
            GLuint radius;
            GLuint gaussianKernel;
        } blur;

        struct {
            GLuint blurRadius;
            GLuint color;
            GLuint offset;
            GLuint contentTexture;
            GLuint gaussianKernel;
        } shadow;
    } m_uniformLocations;
};
#endif

class TextureMapperShaderProgramSimple : public TextureMapperShaderProgram {
public:
    static PassRefPtr<TextureMapperShaderProgramSimple> create()
    {
        return adoptRef(new TextureMapperShaderProgramSimple());
    }

protected:
    TextureMapperShaderProgramSimple();
};

class TextureMapperShaderProgramRectSimple : public TextureMapperShaderProgram {
public:
    static PassRefPtr<TextureMapperShaderProgramRectSimple> create()
    {
        return adoptRef(new TextureMapperShaderProgramRectSimple());
    }

private:
    TextureMapperShaderProgramRectSimple();
};

class TextureMapperShaderProgramOpacityAndMask : public TextureMapperShaderProgram {
public:
    static PassRefPtr<TextureMapperShaderProgramOpacityAndMask> create()
    {
        return adoptRef(new TextureMapperShaderProgramOpacityAndMask());
    }

private:
    TextureMapperShaderProgramOpacityAndMask();
};

class TextureMapperShaderProgramRectOpacityAndMask : public TextureMapperShaderProgram {
public:
    static PassRefPtr<TextureMapperShaderProgramRectOpacityAndMask> create()
    {
        return adoptRef(new TextureMapperShaderProgramRectOpacityAndMask());
    }

private:
    TextureMapperShaderProgramRectOpacityAndMask();
};

class TextureMapperShaderProgramSolidColor : public TextureMapperShaderProgram {
public:
    static PassRefPtr<TextureMapperShaderProgramSolidColor> create()
    {
        return adoptRef(new TextureMapperShaderProgramSolidColor());
    }

    GLint colorLocation() const { return m_colorLocation; }

private:
    TextureMapperShaderProgramSolidColor();
    GLint m_colorLocation;
};

class TextureMapperShaderProgramAntialiasingNoMask : public TextureMapperShaderProgram {
public:
    static PassRefPtr<TextureMapperShaderProgramAntialiasingNoMask> create()
    {
        return adoptRef(new TextureMapperShaderProgramAntialiasingNoMask());
    }

    GLint expandedQuadVerticesInTextureCoordinatesLocation() { return m_expandedQuadVerticesInTextureCordinatesLocation; }
    GLint expandedQuadEdgesInScreenSpaceLocation() { return m_expandedQuadEdgesInScreenSpaceLocation; }

private:
    TextureMapperShaderProgramAntialiasingNoMask();

    GLint m_expandedQuadVerticesInTextureCordinatesLocation;
    GLint m_expandedQuadEdgesInScreenSpaceLocation;
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

    TextureMapperShaderManager();
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

#if ENABLE(CSS_FILTERS)
    typedef HashMap<int, RefPtr<StandardFilterProgram> > FilterMap;
    FilterMap m_filterMap;
#endif

};

}

#endif

#endif // TextureMapperShaderManager_h
