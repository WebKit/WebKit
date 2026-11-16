#ifndef _GLSSHADERRENDERCASE_HPP
#define _GLSSHADERRENDERCASE_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Shader execute test.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "tcuVector.hpp"
#include "tcuMatrix.hpp"
#include "tcuTexture.hpp"
#include "tcuSurface.hpp"
#include "gluRenderContext.hpp"
#include "gluContextInfo.hpp"
#include "gluShaderProgram.hpp"

#include <sstream>
#include <string>

namespace glu
{
class RenderContext;
class Texture2D;
class TextureCube;
class Texture2DArray;
class Texture3D;
} // namespace glu

namespace deqp
{
namespace gls
{

// LineStream \todo [2011-10-17 pyry] Move to proper place!

class LineStream
{
public:
    LineStream(int indent = 0)
    {
        m_indent = indent;
    }
    ~LineStream(void)
    {
    }

    const char *str(void) const
    {
        m_string = m_stream.str();
        return m_string.c_str();
    }
    LineStream &operator<<(const char *line)
    {
        for (int i = 0; i < m_indent; i++)
        {
            m_stream << "\t";
        }
        m_stream << line << "\n";
        return *this;
    }

private:
    int m_indent;
    std::ostringstream m_stream;
    mutable std::string m_string;
};

class QuadGrid;

// TextureBinding

class TextureBinding
{
public:
    enum Type
    {
        TYPE_NONE = 0,
        TYPE_2D,
        TYPE_CUBE_MAP,
        TYPE_2D_ARRAY,
        TYPE_3D,

        TYPE_LAST
    };

    TextureBinding(const glu::Texture2D *tex2D, const tcu::Sampler &sampler);
    TextureBinding(const glu::TextureCube *texCube, const tcu::Sampler &sampler);
    TextureBinding(const glu::Texture2DArray *tex2DArray, const tcu::Sampler &sampler);
    TextureBinding(const glu::Texture3D *tex3D, const tcu::Sampler &sampler);
    TextureBinding(void);

    void setSampler(const tcu::Sampler &sampler);
    void setTexture(const glu::Texture2D *tex2D);
    void setTexture(const glu::TextureCube *texCube);
    void setTexture(const glu::Texture2DArray *tex2DArray);
    void setTexture(const glu::Texture3D *tex3D);

    Type getType(void) const
    {
        return m_type;
    }
    const tcu::Sampler &getSampler(void) const
    {
        return m_sampler;
    }
    const glu::Texture2D *get2D(void) const
    {
        DE_ASSERT(getType() == TYPE_2D);
        return m_binding.tex2D;
    }
    const glu::TextureCube *getCube(void) const
    {
        DE_ASSERT(getType() == TYPE_CUBE_MAP);
        return m_binding.texCube;
    }
    const glu::Texture2DArray *get2DArray(void) const
    {
        DE_ASSERT(getType() == TYPE_2D_ARRAY);
        return m_binding.tex2DArray;
    }
    const glu::Texture3D *get3D(void) const
    {
        DE_ASSERT(getType() == TYPE_3D);
        return m_binding.tex3D;
    }

private:
    Type m_type;
    tcu::Sampler m_sampler;
    union
    {
        const glu::Texture2D *tex2D;
        const glu::TextureCube *texCube;
        const glu::Texture2DArray *tex2DArray;
        const glu::Texture3D *tex3D;
    } m_binding;
};

// ShaderEvalContext.

class ShaderEvalContext
{
public:
    // Limits.
    enum
    {
        MAX_USER_ATTRIBS = 4,
        MAX_TEXTURES     = 4,
    };

    struct ShaderSampler
    {
        tcu::Sampler sampler;
        const tcu::Texture2D *tex2D;
        const tcu::TextureCube *texCube;
        const tcu::Texture2DArray *tex2DArray;
        const tcu::Texture3D *tex3D;

        inline ShaderSampler(void) : tex2D(nullptr), texCube(nullptr), tex2DArray(nullptr), tex3D(nullptr)
        {
        }
    };

    ShaderEvalContext(const QuadGrid &quadGrid);
    ~ShaderEvalContext(void);

    void reset(float sx, float sy);

    // Inputs.
    tcu::Vec4 coords;
    tcu::Vec4 unitCoords;
    tcu::Vec4 constCoords;

    tcu::Vec4 in[MAX_USER_ATTRIBS];
    ShaderSampler textures[MAX_TEXTURES];

    // Output.
    tcu::Vec4 color;
    bool isDiscarded;

    // Functions.
    inline void discard(void)
    {
        isDiscarded = true;
    }
    tcu::Vec4 texture2D(int unitNdx, const tcu::Vec2 &coords);

private:
    const QuadGrid &quadGrid;
};

// ShaderEvalFunc.

typedef void (*ShaderEvalFunc)(ShaderEvalContext &c);

inline void evalCoordsPassthroughX(ShaderEvalContext &c)
{
    c.color.x() = c.coords.x();
}
inline void evalCoordsPassthroughXY(ShaderEvalContext &c)
{
    c.color.xy() = c.coords.swizzle(0, 1);
}
inline void evalCoordsPassthroughXYZ(ShaderEvalContext &c)
{
    c.color.xyz() = c.coords.swizzle(0, 1, 2);
}
inline void evalCoordsPassthrough(ShaderEvalContext &c)
{
    c.color = c.coords;
}
inline void evalCoordsSwizzleWZYX(ShaderEvalContext &c)
{
    c.color = c.coords.swizzle(3, 2, 1, 0);
}

// ShaderEvaluator
// Either inherit a class with overridden evaluate() or just pass in an evalFunc.

class ShaderEvaluator
{
public:
    ShaderEvaluator(void);
    ShaderEvaluator(ShaderEvalFunc evalFunc);
    virtual ~ShaderEvaluator(void);

    virtual void evaluate(ShaderEvalContext &ctx);

private:
    ShaderEvaluator(const ShaderEvaluator &);            // not allowed!
    ShaderEvaluator &operator=(const ShaderEvaluator &); // not allowed!

    ShaderEvalFunc m_evalFunc;
};

// ShaderRenderCase.

class ShaderRenderCase : public tcu::TestCase
{
public:
    ShaderRenderCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &ctxInfo,
                     const char *name, const char *description, bool isVertexCase, ShaderEvalFunc evalFunc,
                     bool useLevel = false);
    ShaderRenderCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const glu::ContextInfo &ctxInfo,
                     const char *name, const char *description, bool isVertexCase, ShaderEvaluator &evaluator,
                     bool useLevel = false);
    virtual ~ShaderRenderCase(void);

    void init(void);
    void deinit(void);

    IterateResult iterate(void);

protected:
    virtual void setupShaderData(void);
    virtual void setup(int programID);
    virtual void setupUniforms(int programID, const tcu::Vec4 &constCoords);

    tcu::IVec2 getViewportSize(void) const;

    class CompileFailed : public tcu::TestError
    {
    public:
        inline CompileFailed(const char *file, int line)
            : tcu::TestError("Failed to compile shader program", nullptr, file, line)
        {
        }
    };

private:
    // Auxiliar class that allows us to work with tcu::Surface and tcu::TextureLevel.
    class InternalSurface
    {
    public:
        explicit InternalSurface(tcu::Surface &surface);
        explicit InternalSurface(tcu::TextureLevel &level);

        int getWidth() const;
        int getHeight() const;

        tcu::ConstPixelBufferAccess getAccess(void) const;
        tcu::PixelBufferAccess getAccess(void);

        void setPixel(int x, int y, const tcu::Vec4 &color) const;

    protected:
        tcu::Surface *m_surfacePtr;
        tcu::TextureLevel *m_levelPtr;
        tcu::PixelBufferAccess m_levelAccess;
    };

    ShaderRenderCase(const ShaderRenderCase &);            // not allowed!
    ShaderRenderCase &operator=(const ShaderRenderCase &); // not allowed!

    void setupDefaultInputs(int programID);

    void render(InternalSurface &result, int programID, const QuadGrid &quadGrid);
    void computeVertexReference(InternalSurface &result, const QuadGrid &quadGrid);
    void computeFragmentReference(InternalSurface &result, const QuadGrid &quadGrid);
    bool compareImages(const tcu::Surface &resImage, const tcu::Surface &refImage, float errorThreshold);

protected:
    glu::RenderContext &m_renderCtx;
    const glu::ContextInfo &m_ctxInfo;

    bool m_isVertexCase;
    bool m_useLevel;
    ShaderEvaluator m_defaultEvaluator;
    ShaderEvaluator &m_evaluator;
    std::string m_vertShaderSource;
    std::string m_fragShaderSource;
    tcu::Vec4 m_clearColor;

    std::vector<tcu::Mat4> m_userAttribTransforms;
    std::vector<TextureBinding> m_textures;

    glu::ShaderProgram *m_program;
    int m_gridSize;
};

// Helpers.
// \todo [2012-04-10 pyry] Move these to separate utility?

const char *getIntUniformName(int number);
const char *getFloatUniformName(int number);
const char *getFloatFractionUniformName(int number);

void setupDefaultUniforms(const glu::RenderContext &context, uint32_t programID);

} // namespace gls
} // namespace deqp

#endif // _GLSSHADERRENDERCASE_HPP
