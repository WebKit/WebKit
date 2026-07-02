#ifndef _GLSTEXTURETESTUTIL_HPP
#define _GLSTEXTURETESTUTIL_HPP
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
 * \brief Texture test utilities.
 *
 * About coordinates:
 *  + Quads consist of 2 triangles, rendered using explicit indices.
 *  + All TextureTestUtil functions and classes expect texture coordinates
 *    for quads to be specified in order (-1, -1), (-1, 1), (1, -1), (1, 1).
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTexture.hpp"
#include "tcuSurface.hpp"
#include "tcuPixelFormat.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuTestContext.hpp"
#include "tcuTestLog.hpp"
#include "tcuCompressedTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTexVerifierUtil.hpp"

#include "gluShaderProgram.hpp"
#include "gluShaderUtil.hpp"
#include "gluTextureTestUtil.hpp"

#include "deInt32.h"

#include <map>

namespace tcu
{
struct LookupPrecision;
struct LodPrecision;
struct TexComparePrecision;
} // namespace tcu

namespace deqp
{
namespace gls
{
namespace TextureTestUtil
{

enum Program
{
    PROGRAM_2D_FLOAT = 0,
    PROGRAM_2D_INT,
    PROGRAM_2D_UINT,
    PROGRAM_2D_SHADOW,

    PROGRAM_2D_FLOAT_BIAS,
    PROGRAM_2D_INT_BIAS,
    PROGRAM_2D_UINT_BIAS,
    PROGRAM_2D_SHADOW_BIAS,

    PROGRAM_1D_FLOAT,
    PROGRAM_1D_INT,
    PROGRAM_1D_UINT,
    PROGRAM_1D_SHADOW,

    PROGRAM_1D_FLOAT_BIAS,
    PROGRAM_1D_INT_BIAS,
    PROGRAM_1D_UINT_BIAS,
    PROGRAM_1D_SHADOW_BIAS,

    PROGRAM_CUBE_FLOAT,
    PROGRAM_CUBE_INT,
    PROGRAM_CUBE_UINT,
    PROGRAM_CUBE_SHADOW,

    PROGRAM_CUBE_FLOAT_BIAS,
    PROGRAM_CUBE_INT_BIAS,
    PROGRAM_CUBE_UINT_BIAS,
    PROGRAM_CUBE_SHADOW_BIAS,

    PROGRAM_1D_ARRAY_FLOAT,
    PROGRAM_1D_ARRAY_INT,
    PROGRAM_1D_ARRAY_UINT,
    PROGRAM_1D_ARRAY_SHADOW,

    PROGRAM_2D_ARRAY_FLOAT,
    PROGRAM_2D_ARRAY_INT,
    PROGRAM_2D_ARRAY_UINT,
    PROGRAM_2D_ARRAY_SHADOW,

    PROGRAM_3D_FLOAT,
    PROGRAM_3D_INT,
    PROGRAM_3D_UINT,

    PROGRAM_3D_FLOAT_BIAS,
    PROGRAM_3D_INT_BIAS,
    PROGRAM_3D_UINT_BIAS,

    PROGRAM_CUBE_ARRAY_FLOAT,
    PROGRAM_CUBE_ARRAY_INT,
    PROGRAM_CUBE_ARRAY_UINT,
    PROGRAM_CUBE_ARRAY_SHADOW,

    PROGRAM_BUFFER_FLOAT,
    PROGRAM_BUFFER_INT,
    PROGRAM_BUFFER_UINT,

    PROGRAM_LAST
};

class ProgramLibrary
{
public:
    ProgramLibrary(const glu::RenderContext &context, tcu::TestLog &log, glu::GLSLVersion glslVersion,
                   glu::Precision texCoordPrecision);
    ~ProgramLibrary(void);

    glu::ShaderProgram *getProgram(Program program);
    void clear(void);
    glu::Precision getTexCoordPrecision();

private:
    ProgramLibrary(const ProgramLibrary &other);
    ProgramLibrary &operator=(const ProgramLibrary &other);

    const glu::RenderContext &m_context;
    tcu::TestLog &m_log;
    glu::GLSLVersion m_glslVersion;
    glu::Precision m_texCoordPrecision;
    std::map<Program, glu::ShaderProgram *> m_programs;
};

class TextureRenderer
{
public:
    TextureRenderer(const glu::RenderContext &context, tcu::TestLog &log, glu::GLSLVersion glslVersion,
                    glu::Precision texCoordPrecision);
    ~TextureRenderer(void);

    void clear(void); //!< Frees allocated resources. Destructor will call clear() as well.

    void renderQuad(int texUnit, const float *texCoord, glu::TextureTestUtil::TextureType texType);
    void renderQuad(int texUnit, const float *texCoord, const glu::TextureTestUtil::RenderParams &params);
    glu::Precision getTexCoordPrecision();

private:
    TextureRenderer(const TextureRenderer &other);
    TextureRenderer &operator=(const TextureRenderer &other);

    const glu::RenderContext &m_renderCtx;
    tcu::TestLog &m_log;
    ProgramLibrary m_programLibrary;
};

class RandomViewport
{
public:
    int x;
    int y;
    int width;
    int height;

    RandomViewport(const tcu::RenderTarget &renderTarget, int preferredWidth, int preferredHeight, uint32_t seed);
};

} // namespace TextureTestUtil
} // namespace gls
} // namespace deqp

#endif // _GLSTEXTURETESTUTIL_HPP
