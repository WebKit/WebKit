/*
 Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)

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

namespace WebCore {

typedef void* ShaderType;

class TextureMapperShaderManager;

class TextureMapperShaderProgram : public RefCounted<TextureMapperShaderProgram> {
public:
    GLuint id() { return m_id; }
    GLuint vertexAttrib() { return m_vertexAttrib; }

    virtual ~TextureMapperShaderProgram();

    template<class T>
    static ShaderType shaderType()
    {
        static int type = 0;
        return &type;
    }

protected:
    void getUniformLocation(GLint& var, const char* name);
    void initializeProgram();
    virtual const char* vertexShaderSource() = 0;
    virtual const char* fragmentShaderSource() = 0;

    GLuint m_id;
    GLuint m_vertexAttrib;
    GLuint m_vertexShader;
    GLuint m_fragmentShader;
};

class TextureMapperShaderProgramSimple : public TextureMapperShaderProgram {
public:
    static PassRefPtr<TextureMapperShaderProgramSimple> create();
    GLint matrixVariable() { return m_matrixVariable; }
    GLint sourceMatrixVariable() { return m_sourceMatrixVariable; }
    GLint sourceTextureVariable() { return m_sourceTextureVariable; }
    GLint opacityVariable() { return m_opacityVariable; }

private:
    virtual const char* vertexShaderSource();
    virtual const char* fragmentShaderSource();
    TextureMapperShaderProgramSimple();
    GLint m_matrixVariable;
    GLint m_sourceMatrixVariable;
    GLint m_sourceTextureVariable;
    GLint m_opacityVariable;
};

class TextureMapperShaderProgramOpacityAndMask : public TextureMapperShaderProgram {
public:
    static PassRefPtr<TextureMapperShaderProgramOpacityAndMask> create();
    GLint sourceMatrixVariable() { return m_sourceMatrixVariable; }
    GLint matrixVariable() { return m_matrixVariable; }
    GLint maskMatrixVariable() { return m_maskMatrixVariable; }
    GLint sourceTextureVariable() { return m_sourceTextureVariable; }
    GLint maskTextureVariable() { return m_maskTextureVariable; }
    GLint opacityVariable() { return m_opacityVariable; }

private:
    static int m_classID;
    virtual const char* vertexShaderSource();
    virtual const char* fragmentShaderSource();
    TextureMapperShaderProgramOpacityAndMask();
    GLint m_sourceMatrixVariable;
    GLint m_matrixVariable;
    GLint m_maskMatrixVariable;
    GLint m_sourceTextureVariable;
    GLint m_maskTextureVariable;
    GLint m_opacityVariable;
};

class TextureMapperShaderManager {
public:
    TextureMapperShaderManager();
    virtual ~TextureMapperShaderManager();

    template<class T>
    PassRefPtr<T> getShaderProgram()
    {
        ShaderType shaderType = TextureMapperShaderProgram::shaderType<T>();
        TextureMapperShaderProgramMap::iterator it = m_textureMapperShaderProgramMap.find(shaderType);
        if (it != m_textureMapperShaderProgramMap.end())
            return static_cast<T*>(it->second.get());

        RefPtr<T> t = T::create();
        m_textureMapperShaderProgramMap.add(shaderType, t);
        return t;
    }

private:
    typedef HashMap<ShaderType, RefPtr<TextureMapperShaderProgram> > TextureMapperShaderProgramMap;
    TextureMapperShaderProgramMap m_textureMapperShaderProgramMap;
};

}

#endif

#endif // TextureMapperShaderManager_h
