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
 *//*--------------------------------------------------------------------*/

#include "glsTextureTestUtil.hpp"
#include "gluDefs.hpp"
#include "gluDrawUtil.hpp"
#include "gluRenderContext.hpp"
#include "deRandom.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTexLookupVerifier.hpp"
#include "tcuTexVerifierUtil.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "qpWatchDog.h"
#include "deStringUtil.hpp"

using std::map;
using std::string;
using std::vector;
using tcu::TestLog;

using namespace glu::TextureTestUtil;

namespace deqp
{
namespace gls
{
namespace TextureTestUtil
{

RandomViewport::RandomViewport(const tcu::RenderTarget &renderTarget, int preferredWidth, int preferredHeight,
                               uint32_t seed)
    : x(0)
    , y(0)
    , width(deMin32(preferredWidth, renderTarget.getWidth()))
    , height(deMin32(preferredHeight, renderTarget.getHeight()))
{
    de::Random rnd(seed);
    x = rnd.getInt(0, renderTarget.getWidth() - width);
    y = rnd.getInt(0, renderTarget.getHeight() - height);
}

ProgramLibrary::ProgramLibrary(const glu::RenderContext &context, tcu::TestLog &log, glu::GLSLVersion glslVersion,
                               glu::Precision texCoordPrecision)
    : m_context(context)
    , m_log(log)
    , m_glslVersion(glslVersion)
    , m_texCoordPrecision(texCoordPrecision)
{
}

ProgramLibrary::~ProgramLibrary(void)
{
    clear();
}

void ProgramLibrary::clear(void)
{
    for (map<Program, glu::ShaderProgram *>::iterator i = m_programs.begin(); i != m_programs.end(); i++)
    {
        delete i->second;
        i->second = nullptr;
    }
    m_programs.clear();
}

glu::ShaderProgram *ProgramLibrary::getProgram(Program program)
{
    if (m_programs.find(program) != m_programs.end())
        return m_programs[program]; // Return from cache.

    static const char *vertShaderTemplate = "${VTX_HEADER}"
                                            "${VTX_IN} highp vec4 a_position;\n"
                                            "${VTX_IN} ${PRECISION} ${TEXCOORD_TYPE} a_texCoord;\n"
                                            "${VTX_OUT} ${PRECISION} ${TEXCOORD_TYPE} v_texCoord;\n"
                                            "\n"
                                            "void main (void)\n"
                                            "{\n"
                                            "    gl_Position = a_position;\n"
                                            "    v_texCoord = a_texCoord;\n"
                                            "}\n";
    static const char *fragShaderTemplate = "${FRAG_HEADER}"
                                            "${FRAG_IN} ${PRECISION} ${TEXCOORD_TYPE} v_texCoord;\n"
                                            "uniform ${PRECISION} float u_bias;\n"
                                            "uniform ${PRECISION} float u_ref;\n"
                                            "uniform ${PRECISION} vec4 u_colorScale;\n"
                                            "uniform ${PRECISION} vec4 u_colorBias;\n"
                                            "uniform ${PRECISION} ${SAMPLER_TYPE} u_sampler;\n"
                                            "\n"
                                            "void main (void)\n"
                                            "{\n"
                                            "    ${FRAG_COLOR} = ${LOOKUP} * u_colorScale + u_colorBias;\n"
                                            "}\n";

    map<string, string> params;

    bool isCube  = de::inRange<int>(program, PROGRAM_CUBE_FLOAT, PROGRAM_CUBE_SHADOW_BIAS);
    bool isArray = de::inRange<int>(program, PROGRAM_2D_ARRAY_FLOAT, PROGRAM_2D_ARRAY_SHADOW) ||
                   de::inRange<int>(program, PROGRAM_1D_ARRAY_FLOAT, PROGRAM_1D_ARRAY_SHADOW);

    bool is1D = de::inRange<int>(program, PROGRAM_1D_FLOAT, PROGRAM_1D_UINT_BIAS) ||
                de::inRange<int>(program, PROGRAM_1D_ARRAY_FLOAT, PROGRAM_1D_ARRAY_SHADOW) ||
                de::inRange<int>(program, PROGRAM_BUFFER_FLOAT, PROGRAM_BUFFER_UINT);

    bool is2D = de::inRange<int>(program, PROGRAM_2D_FLOAT, PROGRAM_2D_UINT_BIAS) ||
                de::inRange<int>(program, PROGRAM_2D_ARRAY_FLOAT, PROGRAM_2D_ARRAY_SHADOW);

    bool is3D        = de::inRange<int>(program, PROGRAM_3D_FLOAT, PROGRAM_3D_UINT_BIAS);
    bool isCubeArray = de::inRange<int>(program, PROGRAM_CUBE_ARRAY_FLOAT, PROGRAM_CUBE_ARRAY_SHADOW);
    bool isBuffer    = de::inRange<int>(program, PROGRAM_BUFFER_FLOAT, PROGRAM_BUFFER_UINT);

    if (m_glslVersion == glu::GLSL_VERSION_100_ES)
    {
        params["FRAG_HEADER"] = "";
        params["VTX_HEADER"]  = "";
        params["VTX_IN"]      = "attribute";
        params["VTX_OUT"]     = "varying";
        params["FRAG_IN"]     = "varying";
        params["FRAG_COLOR"]  = "gl_FragColor";
    }
    else if (m_glslVersion == glu::GLSL_VERSION_300_ES || m_glslVersion == glu::GLSL_VERSION_310_ES ||
             m_glslVersion == glu::GLSL_VERSION_320_ES || m_glslVersion > glu::GLSL_VERSION_330)
    {
        const string version = glu::getGLSLVersionDeclaration(m_glslVersion);
        const char *ext      = nullptr;

        if (glu::glslVersionIsES(m_glslVersion) && m_glslVersion != glu::GLSL_VERSION_320_ES)
        {
            if (isCubeArray)
                ext = "GL_EXT_texture_cube_map_array";
            else if (isBuffer)
                ext = "GL_EXT_texture_buffer";
        }

        params["FRAG_HEADER"] = version + (ext ? string("\n#extension ") + ext + " : require" : string()) +
                                "\nlayout(location = 0) out mediump vec4 dEQP_FragColor;\n";
        params["VTX_HEADER"] = version + "\n";
        params["VTX_IN"]     = "in";
        params["VTX_OUT"]    = "out";
        params["FRAG_IN"]    = "in";
        params["FRAG_COLOR"] = "dEQP_FragColor";
    }
    else
        DE_FATAL("Unsupported version");

    params["PRECISION"] = glu::getPrecisionName(m_texCoordPrecision);

    if (isCubeArray)
        params["TEXCOORD_TYPE"] = "vec4";
    else if (isCube || (is2D && isArray) || is3D)
        params["TEXCOORD_TYPE"] = "vec3";
    else if ((is1D && isArray) || is2D)
        params["TEXCOORD_TYPE"] = "vec2";
    else if (is1D)
        params["TEXCOORD_TYPE"] = "float";
    else
        DE_ASSERT(false);

    const char *sampler = nullptr;
    const char *lookup  = nullptr;

    if (m_glslVersion == glu::GLSL_VERSION_300_ES || m_glslVersion == glu::GLSL_VERSION_310_ES ||
        m_glslVersion == glu::GLSL_VERSION_320_ES || m_glslVersion > glu::GLSL_VERSION_330)
    {
        switch (program)
        {
        case PROGRAM_2D_FLOAT:
            sampler = "sampler2D";
            lookup  = "texture(u_sampler, v_texCoord)";
            break;
        case PROGRAM_2D_INT:
            sampler = "isampler2D";
            lookup  = "vec4(texture(u_sampler, v_texCoord))";
            break;
        case PROGRAM_2D_UINT:
            sampler = "usampler2D";
            lookup  = "vec4(texture(u_sampler, v_texCoord))";
            break;
        case PROGRAM_2D_SHADOW:
            sampler = "sampler2DShadow";
            lookup  = "vec4(texture(u_sampler, vec3(v_texCoord, u_ref)), 0.0, 0.0, 1.0)";
            break;
        case PROGRAM_2D_FLOAT_BIAS:
            sampler = "sampler2D";
            lookup  = "texture(u_sampler, v_texCoord, u_bias)";
            break;
        case PROGRAM_2D_INT_BIAS:
            sampler = "isampler2D";
            lookup  = "vec4(texture(u_sampler, v_texCoord, u_bias))";
            break;
        case PROGRAM_2D_UINT_BIAS:
            sampler = "usampler2D";
            lookup  = "vec4(texture(u_sampler, v_texCoord, u_bias))";
            break;
        case PROGRAM_2D_SHADOW_BIAS:
            sampler = "sampler2DShadow";
            lookup  = "vec4(texture(u_sampler, vec3(v_texCoord, u_ref), u_bias), 0.0, 0.0, 1.0)";
            break;
        case PROGRAM_1D_FLOAT:
            sampler = "sampler1D";
            lookup  = "texture(u_sampler, v_texCoord)";
            break;
        case PROGRAM_1D_INT:
            sampler = "isampler1D";
            lookup  = "vec4(texture(u_sampler, v_texCoord))";
            break;
        case PROGRAM_1D_UINT:
            sampler = "usampler1D";
            lookup  = "vec4(texture(u_sampler, v_texCoord))";
            break;
        case PROGRAM_1D_SHADOW:
            sampler = "sampler1DShadow";
            lookup  = "vec4(texture(u_sampler, vec3(v_texCoord, u_ref)), 0.0, 0.0, 1.0)";
            break;
        case PROGRAM_1D_FLOAT_BIAS:
            sampler = "sampler1D";
            lookup  = "texture(u_sampler, v_texCoord, u_bias)";
            break;
        case PROGRAM_1D_INT_BIAS:
            sampler = "isampler1D";
            lookup  = "vec4(texture(u_sampler, v_texCoord, u_bias))";
            break;
        case PROGRAM_1D_UINT_BIAS:
            sampler = "usampler1D";
            lookup  = "vec4(texture(u_sampler, v_texCoord, u_bias))";
            break;
        case PROGRAM_1D_SHADOW_BIAS:
            sampler = "sampler1DShadow";
            lookup  = "vec4(texture(u_sampler, vec3(v_texCoord, u_ref), u_bias), 0.0, 0.0, 1.0)";
            break;
        case PROGRAM_CUBE_FLOAT:
            sampler = "samplerCube";
            lookup  = "texture(u_sampler, v_texCoord)";
            break;
        case PROGRAM_CUBE_INT:
            sampler = "isamplerCube";
            lookup  = "vec4(texture(u_sampler, v_texCoord))";
            break;
        case PROGRAM_CUBE_UINT:
            sampler = "usamplerCube";
            lookup  = "vec4(texture(u_sampler, v_texCoord))";
            break;
        case PROGRAM_CUBE_SHADOW:
            sampler = "samplerCubeShadow";
            lookup  = "vec4(texture(u_sampler, vec4(v_texCoord, u_ref)), 0.0, 0.0, 1.0)";
            break;
        case PROGRAM_CUBE_FLOAT_BIAS:
            sampler = "samplerCube";
            lookup  = "texture(u_sampler, v_texCoord, u_bias)";
            break;
        case PROGRAM_CUBE_INT_BIAS:
            sampler = "isamplerCube";
            lookup  = "vec4(texture(u_sampler, v_texCoord, u_bias))";
            break;
        case PROGRAM_CUBE_UINT_BIAS:
            sampler = "usamplerCube";
            lookup  = "vec4(texture(u_sampler, v_texCoord, u_bias))";
            break;
        case PROGRAM_CUBE_SHADOW_BIAS:
            sampler = "samplerCubeShadow";
            lookup  = "vec4(texture(u_sampler, vec4(v_texCoord, u_ref), u_bias), 0.0, 0.0, 1.0)";
            break;
        case PROGRAM_2D_ARRAY_FLOAT:
            sampler = "sampler2DArray";
            lookup  = "texture(u_sampler, v_texCoord)";
            break;
        case PROGRAM_2D_ARRAY_INT:
            sampler = "isampler2DArray";
            lookup  = "vec4(texture(u_sampler, v_texCoord))";
            break;
        case PROGRAM_2D_ARRAY_UINT:
            sampler = "usampler2DArray";
            lookup  = "vec4(texture(u_sampler, v_texCoord))";
            break;
        case PROGRAM_2D_ARRAY_SHADOW:
            sampler = "sampler2DArrayShadow";
            lookup  = "vec4(texture(u_sampler, vec4(v_texCoord, u_ref)), 0.0, 0.0, 1.0)";
            break;
        case PROGRAM_3D_FLOAT:
            sampler = "sampler3D";
            lookup  = "texture(u_sampler, v_texCoord)";
            break;
        case PROGRAM_3D_INT:
            sampler = "isampler3D";
            lookup  = "vec4(texture(u_sampler, v_texCoord))";
            break;
        case PROGRAM_3D_UINT:
            sampler = "usampler3D";
            lookup  = "vec4(texture(u_sampler, v_texCoord))";
            break;
        case PROGRAM_3D_FLOAT_BIAS:
            sampler = "sampler3D";
            lookup  = "texture(u_sampler, v_texCoord, u_bias)";
            break;
        case PROGRAM_3D_INT_BIAS:
            sampler = "isampler3D";
            lookup  = "vec4(texture(u_sampler, v_texCoord, u_bias))";
            break;
        case PROGRAM_3D_UINT_BIAS:
            sampler = "usampler3D";
            lookup  = "vec4(texture(u_sampler, v_texCoord, u_bias))";
            break;
        case PROGRAM_CUBE_ARRAY_FLOAT:
            sampler = "samplerCubeArray";
            lookup  = "texture(u_sampler, v_texCoord)";
            break;
        case PROGRAM_CUBE_ARRAY_INT:
            sampler = "isamplerCubeArray";
            lookup  = "vec4(texture(u_sampler, v_texCoord))";
            break;
        case PROGRAM_CUBE_ARRAY_UINT:
            sampler = "usamplerCubeArray";
            lookup  = "vec4(texture(u_sampler, v_texCoord))";
            break;
        case PROGRAM_CUBE_ARRAY_SHADOW:
            sampler = "samplerCubeArrayShadow";
            lookup  = "vec4(texture(u_sampler, vec4(v_texCoord, u_ref)), 0.0, 0.0, 1.0)";
            break;
        case PROGRAM_1D_ARRAY_FLOAT:
            sampler = "sampler1DArray";
            lookup  = "texture(u_sampler, v_texCoord)";
            break;
        case PROGRAM_1D_ARRAY_INT:
            sampler = "isampler1DArray";
            lookup  = "vec4(texture(u_sampler, v_texCoord))";
            break;
        case PROGRAM_1D_ARRAY_UINT:
            sampler = "usampler1DArray";
            lookup  = "vec4(texture(u_sampler, v_texCoord))";
            break;
        case PROGRAM_1D_ARRAY_SHADOW:
            sampler = "sampler1DArrayShadow";
            lookup  = "vec4(texture(u_sampler, vec4(v_texCoord, u_ref)), 0.0, 0.0, 1.0)";
            break;
        case PROGRAM_BUFFER_FLOAT:
            sampler = "samplerBuffer";
            lookup  = "texelFetch(u_sampler, int(v_texCoord))";
            break;
        case PROGRAM_BUFFER_INT:
            sampler = "isamplerBuffer";
            lookup  = "vec4(texelFetch(u_sampler, int(v_texCoord)))";
            break;
        case PROGRAM_BUFFER_UINT:
            sampler = "usamplerBuffer";
            lookup  = "vec4(texelFetch(u_sampler, int(v_texCoord)))";
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else if (m_glslVersion == glu::GLSL_VERSION_100_ES)
    {
        sampler = isCube ? "samplerCube" : "sampler2D";

        switch (program)
        {
        case PROGRAM_2D_FLOAT:
            lookup = "texture2D(u_sampler, v_texCoord)";
            break;
        case PROGRAM_2D_FLOAT_BIAS:
            lookup = "texture2D(u_sampler, v_texCoord, u_bias)";
            break;
        case PROGRAM_CUBE_FLOAT:
            lookup = "textureCube(u_sampler, v_texCoord)";
            break;
        case PROGRAM_CUBE_FLOAT_BIAS:
            lookup = "textureCube(u_sampler, v_texCoord, u_bias)";
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else
        DE_FATAL("Unsupported version");

    params["SAMPLER_TYPE"] = sampler;
    params["LOOKUP"]       = lookup;

    std::string vertSrc = tcu::StringTemplate(vertShaderTemplate).specialize(params);
    std::string fragSrc = tcu::StringTemplate(fragShaderTemplate).specialize(params);

    glu::ShaderProgram *progObj = new glu::ShaderProgram(m_context, glu::makeVtxFragSources(vertSrc, fragSrc));
    if (!progObj->isOk())
    {
        m_log << *progObj;
        delete progObj;
        TCU_FAIL("Failed to compile shader program");
    }

    try
    {
        m_programs[program] = progObj;
    }
    catch (...)
    {
        delete progObj;
        throw;
    }

    return progObj;
}

glu::Precision ProgramLibrary::getTexCoordPrecision()
{
    return m_texCoordPrecision;
}

TextureRenderer::TextureRenderer(const glu::RenderContext &context, tcu::TestLog &log, glu::GLSLVersion glslVersion,
                                 glu::Precision texCoordPrecision)
    : m_renderCtx(context)
    , m_log(log)
    , m_programLibrary(context, log, glslVersion, texCoordPrecision)
{
}

TextureRenderer::~TextureRenderer(void)
{
    clear();
}

void TextureRenderer::clear(void)
{
    m_programLibrary.clear();
}

void TextureRenderer::renderQuad(int texUnit, const float *texCoord, TextureType texType)
{
    renderQuad(texUnit, texCoord, RenderParams(texType));
}

void TextureRenderer::renderQuad(int texUnit, const float *texCoord, const RenderParams &params)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    tcu::Vec4 wCoord         = params.flags & RenderParams::PROJECTED ? params.w : tcu::Vec4(1.0f);
    bool useBias             = !!(params.flags & RenderParams::USE_BIAS);
    bool logUniforms         = !!(params.flags & RenderParams::LOG_UNIFORMS);

    // Render quad with texture.
    float position[]                = {-1.0f * wCoord.x(), -1.0f * wCoord.x(), 0.0f, wCoord.x(),
                                       -1.0f * wCoord.y(), +1.0f * wCoord.y(), 0.0f, wCoord.y(),
                                       +1.0f * wCoord.z(), -1.0f * wCoord.z(), 0.0f, wCoord.z(),
                                       +1.0f * wCoord.w(), +1.0f * wCoord.w(), 0.0f, wCoord.w()};
    static const uint16_t indices[] = {0, 1, 2, 2, 1, 3};

    Program progSpec = PROGRAM_LAST;
    int numComps     = 0;
    if (params.texType == TEXTURETYPE_2D)
    {
        numComps = 2;

        switch (params.samplerType)
        {
        case SAMPLERTYPE_FLOAT:
            progSpec = useBias ? PROGRAM_2D_FLOAT_BIAS : PROGRAM_2D_FLOAT;
            break;
        case SAMPLERTYPE_INT:
            progSpec = useBias ? PROGRAM_2D_INT_BIAS : PROGRAM_2D_INT;
            break;
        case SAMPLERTYPE_UINT:
            progSpec = useBias ? PROGRAM_2D_UINT_BIAS : PROGRAM_2D_UINT;
            break;
        case SAMPLERTYPE_SHADOW:
            progSpec = useBias ? PROGRAM_2D_SHADOW_BIAS : PROGRAM_2D_SHADOW;
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else if (params.texType == TEXTURETYPE_1D)
    {
        numComps = 1;

        switch (params.samplerType)
        {
        case SAMPLERTYPE_FLOAT:
            progSpec = useBias ? PROGRAM_1D_FLOAT_BIAS : PROGRAM_1D_FLOAT;
            break;
        case SAMPLERTYPE_INT:
            progSpec = useBias ? PROGRAM_1D_INT_BIAS : PROGRAM_1D_INT;
            break;
        case SAMPLERTYPE_UINT:
            progSpec = useBias ? PROGRAM_1D_UINT_BIAS : PROGRAM_1D_UINT;
            break;
        case SAMPLERTYPE_SHADOW:
            progSpec = useBias ? PROGRAM_1D_SHADOW_BIAS : PROGRAM_1D_SHADOW;
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else if (params.texType == TEXTURETYPE_CUBE)
    {
        numComps = 3;

        switch (params.samplerType)
        {
        case SAMPLERTYPE_FLOAT:
            progSpec = useBias ? PROGRAM_CUBE_FLOAT_BIAS : PROGRAM_CUBE_FLOAT;
            break;
        case SAMPLERTYPE_INT:
            progSpec = useBias ? PROGRAM_CUBE_INT_BIAS : PROGRAM_CUBE_INT;
            break;
        case SAMPLERTYPE_UINT:
            progSpec = useBias ? PROGRAM_CUBE_UINT_BIAS : PROGRAM_CUBE_UINT;
            break;
        case SAMPLERTYPE_SHADOW:
            progSpec = useBias ? PROGRAM_CUBE_SHADOW_BIAS : PROGRAM_CUBE_SHADOW;
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else if (params.texType == TEXTURETYPE_3D)
    {
        numComps = 3;

        switch (params.samplerType)
        {
        case SAMPLERTYPE_FLOAT:
            progSpec = useBias ? PROGRAM_3D_FLOAT_BIAS : PROGRAM_3D_FLOAT;
            break;
        case SAMPLERTYPE_INT:
            progSpec = useBias ? PROGRAM_3D_INT_BIAS : PROGRAM_3D_INT;
            break;
        case SAMPLERTYPE_UINT:
            progSpec = useBias ? PROGRAM_3D_UINT_BIAS : PROGRAM_3D_UINT;
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else if (params.texType == TEXTURETYPE_2D_ARRAY)
    {
        DE_ASSERT(!useBias); // \todo [2012-02-17 pyry] Support bias.

        numComps = 3;

        switch (params.samplerType)
        {
        case SAMPLERTYPE_FLOAT:
            progSpec = PROGRAM_2D_ARRAY_FLOAT;
            break;
        case SAMPLERTYPE_INT:
            progSpec = PROGRAM_2D_ARRAY_INT;
            break;
        case SAMPLERTYPE_UINT:
            progSpec = PROGRAM_2D_ARRAY_UINT;
            break;
        case SAMPLERTYPE_SHADOW:
            progSpec = PROGRAM_2D_ARRAY_SHADOW;
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else if (params.texType == TEXTURETYPE_CUBE_ARRAY)
    {
        DE_ASSERT(!useBias);

        numComps = 4;

        switch (params.samplerType)
        {
        case SAMPLERTYPE_FLOAT:
            progSpec = PROGRAM_CUBE_ARRAY_FLOAT;
            break;
        case SAMPLERTYPE_INT:
            progSpec = PROGRAM_CUBE_ARRAY_INT;
            break;
        case SAMPLERTYPE_UINT:
            progSpec = PROGRAM_CUBE_ARRAY_UINT;
            break;
        case SAMPLERTYPE_SHADOW:
            progSpec = PROGRAM_CUBE_ARRAY_SHADOW;
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else if (params.texType == TEXTURETYPE_1D_ARRAY)
    {
        DE_ASSERT(!useBias); // \todo [2012-02-17 pyry] Support bias.

        numComps = 2;

        switch (params.samplerType)
        {
        case SAMPLERTYPE_FLOAT:
            progSpec = PROGRAM_1D_ARRAY_FLOAT;
            break;
        case SAMPLERTYPE_INT:
            progSpec = PROGRAM_1D_ARRAY_INT;
            break;
        case SAMPLERTYPE_UINT:
            progSpec = PROGRAM_1D_ARRAY_UINT;
            break;
        case SAMPLERTYPE_SHADOW:
            progSpec = PROGRAM_1D_ARRAY_SHADOW;
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else if (params.texType == TEXTURETYPE_BUFFER)
    {
        numComps = 1;

        switch (params.samplerType)
        {
        case SAMPLERTYPE_FETCH_FLOAT:
            progSpec = PROGRAM_BUFFER_FLOAT;
            break;
        case SAMPLERTYPE_FETCH_INT:
            progSpec = PROGRAM_BUFFER_INT;
            break;
        case SAMPLERTYPE_FETCH_UINT:
            progSpec = PROGRAM_BUFFER_UINT;
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else
        DE_ASSERT(false);

    glu::ShaderProgram *program = m_programLibrary.getProgram(progSpec);

    // \todo [2012-09-26 pyry] Move to ProgramLibrary and log unique programs only(?)
    if (params.flags & RenderParams::LOG_PROGRAMS)
        m_log << *program;

    GLU_EXPECT_NO_ERROR(gl.getError(), "Set vertex attributes");

    // Program and uniforms.
    uint32_t prog = program->getProgram();
    gl.useProgram(prog);

    gl.uniform1i(gl.getUniformLocation(prog, "u_sampler"), texUnit);
    if (logUniforms)
        m_log << TestLog::Message << "u_sampler = " << texUnit << TestLog::EndMessage;

    if (useBias)
    {
        gl.uniform1f(gl.getUniformLocation(prog, "u_bias"), params.bias);
        if (logUniforms)
            m_log << TestLog::Message << "u_bias = " << params.bias << TestLog::EndMessage;
    }

    if (params.samplerType == SAMPLERTYPE_SHADOW)
    {
        gl.uniform1f(gl.getUniformLocation(prog, "u_ref"), params.ref);
        if (logUniforms)
            m_log << TestLog::Message << "u_ref = " << params.ref << TestLog::EndMessage;
    }

    gl.uniform4fv(gl.getUniformLocation(prog, "u_colorScale"), 1, params.colorScale.getPtr());
    gl.uniform4fv(gl.getUniformLocation(prog, "u_colorBias"), 1, params.colorBias.getPtr());

    if (logUniforms)
    {
        m_log << TestLog::Message << "u_colorScale = " << params.colorScale << TestLog::EndMessage;
        m_log << TestLog::Message << "u_colorBias = " << params.colorBias << TestLog::EndMessage;
    }

    GLU_EXPECT_NO_ERROR(gl.getError(), "Set program state");

    {
        const glu::VertexArrayBinding vertexArrays[] = {glu::va::Float("a_position", 4, 4, 0, &position[0]),
                                                        glu::va::Float("a_texCoord", numComps, 4, 0, texCoord)};
        glu::draw(m_renderCtx, prog, DE_LENGTH_OF_ARRAY(vertexArrays), &vertexArrays[0],
                  glu::pr::Triangles(DE_LENGTH_OF_ARRAY(indices), &indices[0]));
    }
}

glu::Precision TextureRenderer::getTexCoordPrecision()
{
    return m_programLibrary.getTexCoordPrecision();
}

} // namespace TextureTestUtil
} // namespace gls
} // namespace deqp
