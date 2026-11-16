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
 * \brief Random shader test case.
 *//*--------------------------------------------------------------------*/

#include "glsRandomShaderCase.hpp"

#include "gluShaderProgram.hpp"
#include "gluPixelTransfer.hpp"
#include "gluTextureUtil.hpp"
#include "gluStrUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"

#include "deRandom.hpp"
#include "deStringUtil.hpp"

#include "rsgProgramGenerator.hpp"
#include "rsgProgramExecutor.hpp"
#include "rsgUtils.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuRenderTarget.hpp"

#include "glw.h"
#include "glwFunctions.hpp"

using std::map;
using std::pair;
using std::string;
using std::vector;

namespace deqp
{
namespace gls
{

enum
{
    VIEWPORT_WIDTH  = 64,
    VIEWPORT_HEIGHT = 64,

    TEXTURE_2D_WIDTH     = 64,
    TEXTURE_2D_HEIGHT    = 64,
    TEXTURE_2D_FORMAT    = GL_RGBA,
    TEXTURE_2D_DATA_TYPE = GL_UNSIGNED_BYTE,

    TEXTURE_CUBE_SIZE      = 16,
    TEXTURE_CUBE_FORMAT    = GL_RGBA,
    TEXTURE_CUBE_DATA_TYPE = GL_UNSIGNED_BYTE,

    TEXTURE_WRAP_S = GL_CLAMP_TO_EDGE,
    TEXTURE_WRAP_T = GL_CLAMP_TO_EDGE,

    TEXTURE_MIN_FILTER = GL_LINEAR,
    TEXTURE_MAG_FILTER = GL_LINEAR
};

VertexArray::VertexArray(const rsg::ShaderInput *input, int numVertices)
    : m_input(input)
    , m_vertices(input->getVariable()->getType().getNumElements() * numVertices)
{
}

TextureManager::TextureManager(void)
{
}

TextureManager::~TextureManager(void)
{
}

void TextureManager::bindTexture(int unit, const glu::Texture2D *tex2D)
{
    m_tex2D[unit] = tex2D;
}

void TextureManager::bindTexture(int unit, const glu::TextureCube *texCube)
{
    m_texCube[unit] = texCube;
}

inline vector<pair<int, const glu::Texture2D *>> TextureManager::getBindings2D(void) const
{
    vector<pair<int, const glu::Texture2D *>> bindings;
    for (map<int, const glu::Texture2D *>::const_iterator i = m_tex2D.begin(); i != m_tex2D.end(); i++)
        bindings.push_back(*i);
    return bindings;
}

inline vector<pair<int, const glu::TextureCube *>> TextureManager::getBindingsCube(void) const
{
    vector<pair<int, const glu::TextureCube *>> bindings;
    for (map<int, const glu::TextureCube *>::const_iterator i = m_texCube.begin(); i != m_texCube.end(); i++)
        bindings.push_back(*i);
    return bindings;
}

RandomShaderCase::RandomShaderCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                   const char *description, const rsg::ProgramParameters &params)
    : tcu::TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_parameters(params)
    , m_gridWidth(1)
    , m_gridHeight(1)
    , m_vertexShader(rsg::Shader::TYPE_VERTEX)
    , m_fragmentShader(rsg::Shader::TYPE_FRAGMENT)
    , m_tex2D(nullptr)
    , m_texCube(nullptr)
{
}

RandomShaderCase::~RandomShaderCase(void)
{
    delete m_tex2D;
    delete m_texCube;
}

void RandomShaderCase::init(void)
{
    // Generate shaders
    rsg::ProgramGenerator programGenerator;
    programGenerator.generate(m_parameters, m_vertexShader, m_fragmentShader);

    checkShaderLimits(m_vertexShader);
    checkShaderLimits(m_fragmentShader);
    checkProgramLimits(m_vertexShader, m_fragmentShader);

    // Compute uniform values
    std::vector<const rsg::ShaderInput *> unifiedUniforms;
    de::Random rnd(m_parameters.seed);
    rsg::computeUnifiedUniforms(m_vertexShader, m_fragmentShader, unifiedUniforms);
    rsg::computeUniformValues(rnd, m_uniforms, unifiedUniforms);

    // Generate vertices
    const vector<rsg::ShaderInput *> &inputs = m_vertexShader.getInputs();
    int numVertices                          = (m_gridWidth + 1) * (m_gridHeight + 1);

    for (vector<rsg::ShaderInput *>::const_iterator i = inputs.begin(); i != inputs.end(); i++)
    {
        const rsg::ShaderInput *input         = *i;
        rsg::ConstValueRangeAccess valueRange = input->getValueRange();
        int numComponents                     = input->getVariable()->getType().getNumElements();
        VertexArray vtxArray(input, numVertices);
        bool isPosition = string(input->getVariable()->getName()) == "dEQP_Position";

        TCU_CHECK(input->getVariable()->getType().getBaseType() == rsg::VariableType::TYPE_FLOAT);

        for (int vtxNdx = 0; vtxNdx < numVertices; vtxNdx++)
        {
            int y      = vtxNdx / (m_gridWidth + 1);
            int x      = vtxNdx - y * (m_gridWidth + 1);
            float xf   = (float)x / (float)m_gridWidth;
            float yf   = (float)y / (float)m_gridHeight;
            float *dst = &vtxArray.getVertices()[vtxNdx * numComponents];

            if (isPosition)
            {
                // Position attribute gets special interpolation handling.
                DE_ASSERT(numComponents == 4);
                dst[0] = -1.0f + xf * 2.0f;
                dst[1] = 1.0f + yf * -2.0f;
                dst[2] = 0.0f;
                dst[3] = 1.0f;
            }
            else
            {
                for (int compNdx = 0; compNdx < numComponents; compNdx++)
                {
                    float minVal = valueRange.getMin().component(compNdx).asFloat();
                    float maxVal = valueRange.getMax().component(compNdx).asFloat();
                    float xd, yd;

                    rsg::getVertexInterpolationCoords(xd, yd, xf, yf, compNdx);

                    float f = (xd + yd) / 2.0f;

                    dst[compNdx] = minVal + f * (maxVal - minVal);
                }
            }
        }

        m_vertexArrays.push_back(vtxArray);
    }

    // Generate indices
    int numQuads   = m_gridWidth * m_gridHeight;
    int numIndices = numQuads * 6;
    m_indices.resize(numIndices);
    for (int quadNdx = 0; quadNdx < numQuads; quadNdx++)
    {
        int quadY = quadNdx / (m_gridWidth);
        int quadX = quadNdx - quadY * m_gridWidth;

        m_indices[quadNdx * 6 + 0] = (uint16_t)(quadX + quadY * (m_gridWidth + 1));
        m_indices[quadNdx * 6 + 1] = (uint16_t)(quadX + (quadY + 1) * (m_gridWidth + 1));
        m_indices[quadNdx * 6 + 2] = (uint16_t)(quadX + quadY * (m_gridWidth + 1) + 1);
        m_indices[quadNdx * 6 + 3] = (uint16_t)(m_indices[quadNdx * 6 + 2]);
        m_indices[quadNdx * 6 + 4] = (uint16_t)(m_indices[quadNdx * 6 + 1]);
        m_indices[quadNdx * 6 + 5] = (uint16_t)(quadX + (quadY + 1) * (m_gridWidth + 1) + 1);
    }

    // Create textures.
    for (vector<rsg::VariableValue>::const_iterator uniformIter = m_uniforms.begin(); uniformIter != m_uniforms.end();
         uniformIter++)
    {
        const rsg::VariableType &type = uniformIter->getVariable()->getType();

        if (!type.isSampler())
            continue;

        int unitNdx = uniformIter->getValue().asInt(0);

        if (type == rsg::VariableType(rsg::VariableType::TYPE_SAMPLER_2D, 1))
            m_texManager.bindTexture(unitNdx, getTex2D());
        else if (type == rsg::VariableType(rsg::VariableType::TYPE_SAMPLER_CUBE, 1))
            m_texManager.bindTexture(unitNdx, getTexCube());
        else
            DE_ASSERT(false);
    }
}

static int getNumSamplerUniforms(const std::vector<rsg::ShaderInput *> &uniforms)
{
    int numSamplers = 0;

    for (std::vector<rsg::ShaderInput *>::const_iterator it = uniforms.begin(); it != uniforms.end(); ++it)
    {
        if ((*it)->getVariable()->getType().isSampler())
            ++numSamplers;
    }

    return numSamplers;
}

void RandomShaderCase::checkShaderLimits(const rsg::Shader &shader) const
{
    const int numRequiredSamplers = getNumSamplerUniforms(shader.getUniforms());

    if (numRequiredSamplers > 0)
    {
        const GLenum pname = (shader.getType() == rsg::Shader::TYPE_VERTEX) ? (GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS) :
                                                                              (GL_MAX_TEXTURE_IMAGE_UNITS);
        int numSupported   = -1;
        GLenum error;

        m_renderCtx.getFunctions().getIntegerv(pname, &numSupported);
        error = m_renderCtx.getFunctions().getError();

        if (error != GL_NO_ERROR)
            throw tcu::TestError("Limit query failed: " + de::toString(glu::getErrorStr(error)));

        if (numSupported < numRequiredSamplers)
            throw tcu::NotSupportedError("Shader requires " + de::toString(numRequiredSamplers) +
                                         " sampler(s). Implementation supports " + de::toString(numSupported));
    }
}

void RandomShaderCase::checkProgramLimits(const rsg::Shader &vtxShader, const rsg::Shader &frgShader) const
{
    const int numRequiredCombinedSamplers =
        getNumSamplerUniforms(vtxShader.getUniforms()) + getNumSamplerUniforms(frgShader.getUniforms());

    if (numRequiredCombinedSamplers > 0)
    {
        int numSupported = -1;
        GLenum error;

        m_renderCtx.getFunctions().getIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &numSupported);
        error = m_renderCtx.getFunctions().getError();

        if (error != GL_NO_ERROR)
            throw tcu::TestError("Limit query failed: " + de::toString(glu::getErrorStr(error)));

        if (numSupported < numRequiredCombinedSamplers)
            throw tcu::NotSupportedError("Program requires " + de::toString(numRequiredCombinedSamplers) +
                                         " sampler(s). Implementation supports " + de::toString(numSupported));
    }
}

const glu::Texture2D *RandomShaderCase::getTex2D(void)
{
    if (!m_tex2D)
    {
        m_tex2D = new glu::Texture2D(m_renderCtx, TEXTURE_2D_FORMAT, TEXTURE_2D_DATA_TYPE, TEXTURE_2D_WIDTH,
                                     TEXTURE_2D_HEIGHT);

        m_tex2D->getRefTexture().allocLevel(0);
        tcu::fillWithComponentGradients(m_tex2D->getRefTexture().getLevel(0), tcu::Vec4(-1.0f, -1.0f, -1.0f, 2.0f),
                                        tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f));
        m_tex2D->upload();

        // Setup parameters.
        glBindTexture(GL_TEXTURE_2D, m_tex2D->getGLTexture());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, TEXTURE_WRAP_S);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, TEXTURE_WRAP_T);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TEXTURE_MIN_FILTER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TEXTURE_MAG_FILTER);

        GLU_CHECK();
    }

    return m_tex2D;
}

const glu::TextureCube *RandomShaderCase::getTexCube(void)
{
    if (!m_texCube)
    {
        m_texCube = new glu::TextureCube(m_renderCtx, TEXTURE_CUBE_FORMAT, TEXTURE_CUBE_DATA_TYPE, TEXTURE_CUBE_SIZE);

        static const tcu::Vec4 gradients[tcu::CUBEFACE_LAST][2] = {
            {tcu::Vec4(-1.0f, -1.0f, -1.0f, 2.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)}, // negative x
            {tcu::Vec4(0.0f, -1.0f, -1.0f, 2.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)},  // positive x
            {tcu::Vec4(-1.0f, 0.0f, -1.0f, 2.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)},  // negative y
            {tcu::Vec4(-1.0f, -1.0f, 0.0f, 2.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)},  // positive y
            {tcu::Vec4(-1.0f, -1.0f, -1.0f, 0.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f)}, // negative z
            {tcu::Vec4(0.0f, 0.0f, 0.0f, 2.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)}     // positive z
        };

        // Fill level 0.
        for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
        {
            m_texCube->getRefTexture().allocLevel((tcu::CubeFace)face, 0);
            tcu::fillWithComponentGradients(m_texCube->getRefTexture().getLevelFace(0, (tcu::CubeFace)face),
                                            gradients[face][0], gradients[face][1]);
        }

        m_texCube->upload();

        // Setup parameters.
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_texCube->getGLTexture());
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, TEXTURE_WRAP_S);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, TEXTURE_WRAP_T);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, TEXTURE_MIN_FILTER);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, TEXTURE_MAG_FILTER);

        GLU_CHECK();
    }

    return m_texCube;
}

void RandomShaderCase::deinit(void)
{
    delete m_tex2D;
    delete m_texCube;

    m_tex2D   = nullptr;
    m_texCube = nullptr;

    // Free up memory
    m_vertexArrays.clear();
    m_indices.clear();
}

namespace
{

void setUniformValue(int location, rsg::ConstValueAccess value)
{
    DE_STATIC_ASSERT(sizeof(rsg::Scalar) == sizeof(float));
    DE_STATIC_ASSERT(sizeof(rsg::Scalar) == sizeof(int));

    switch (value.getType().getBaseType())
    {
    case rsg::VariableType::TYPE_FLOAT:
        switch (value.getType().getNumElements())
        {
        case 1:
            glUniform1fv(location, 1, (float *)value.value().getValuePtr());
            break;
        case 2:
            glUniform2fv(location, 1, (float *)value.value().getValuePtr());
            break;
        case 3:
            glUniform3fv(location, 1, (float *)value.value().getValuePtr());
            break;
        case 4:
            glUniform4fv(location, 1, (float *)value.value().getValuePtr());
            break;
        default:
            TCU_FAIL("Unsupported type");
        }
        break;

    case rsg::VariableType::TYPE_INT:
    case rsg::VariableType::TYPE_BOOL:
    case rsg::VariableType::TYPE_SAMPLER_2D:
    case rsg::VariableType::TYPE_SAMPLER_CUBE:
        switch (value.getType().getNumElements())
        {
        case 1:
            glUniform1iv(location, 1, (int *)value.value().getValuePtr());
            break;
        case 2:
            glUniform2iv(location, 1, (int *)value.value().getValuePtr());
            break;
        case 3:
            glUniform3iv(location, 1, (int *)value.value().getValuePtr());
            break;
        case 4:
            glUniform4iv(location, 1, (int *)value.value().getValuePtr());
            break;
        default:
            TCU_FAIL("Unsupported type");
        }
        break;

    default:
        TCU_FAIL("Unsupported type");
    }
}

tcu::MessageBuilder &operator<<(tcu::MessageBuilder &message, rsg::ConstValueAccess value)
{
    const char *scalarType = nullptr;
    const char *vecType    = nullptr;

    switch (value.getType().getBaseType())
    {
    case rsg::VariableType::TYPE_FLOAT:
        scalarType = "float";
        vecType    = "vec";
        break;
    case rsg::VariableType::TYPE_INT:
        scalarType = "int";
        vecType    = "ivec";
        break;
    case rsg::VariableType::TYPE_BOOL:
        scalarType = "bool";
        vecType    = "bvec";
        break;
    case rsg::VariableType::TYPE_SAMPLER_2D:
        scalarType = "sampler2D";
        break;
    case rsg::VariableType::TYPE_SAMPLER_CUBE:
        scalarType = "samplerCube";
        break;
    default:
        TCU_FAIL("Unsupported type.");
    }

    int numElements = value.getType().getNumElements();
    if (numElements == 1)
        message << scalarType << "(";
    else
        message << vecType << numElements << "(";

    for (int elementNdx = 0; elementNdx < numElements; elementNdx++)
    {
        if (elementNdx > 0)
            message << ", ";

        switch (value.getType().getBaseType())
        {
        case rsg::VariableType::TYPE_FLOAT:
            message << value.component(elementNdx).asFloat();
            break;
        case rsg::VariableType::TYPE_INT:
            message << value.component(elementNdx).asInt();
            break;
        case rsg::VariableType::TYPE_BOOL:
            message << (value.component(elementNdx).asBool() ? "true" : "false");
            break;
        case rsg::VariableType::TYPE_SAMPLER_2D:
            message << value.component(elementNdx).asInt();
            break;
        case rsg::VariableType::TYPE_SAMPLER_CUBE:
            message << value.component(elementNdx).asInt();
            break;
        default:
            DE_ASSERT(false);
        }
    }

    message << ")";

    return message;
}

tcu::MessageBuilder &operator<<(tcu::MessageBuilder &message, rsg::ConstValueRangeAccess valueRange)
{
    return message << valueRange.getMin() << " -> " << valueRange.getMax();
}

} // namespace

RandomShaderCase::IterateResult RandomShaderCase::iterate(void)
{
    tcu::TestLog &log = m_testCtx.getLog();

    // Compile program
    glu::ShaderProgram program(m_renderCtx,
                               glu::makeVtxFragSources(m_vertexShader.getSource(), m_fragmentShader.getSource()));
    log << program;

    if (!program.isOk())
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to compile shader");
        return STOP;
    }

    // Compute random viewport
    de::Random rnd(m_parameters.seed);
    int viewportWidth  = de::min<int>(VIEWPORT_WIDTH, m_renderCtx.getRenderTarget().getWidth());
    int viewportHeight = de::min<int>(VIEWPORT_HEIGHT, m_renderCtx.getRenderTarget().getHeight());
    int viewportX      = rnd.getInt(0, m_renderCtx.getRenderTarget().getWidth() - viewportWidth);
    int viewportY      = rnd.getInt(0, m_renderCtx.getRenderTarget().getHeight() - viewportHeight);
    bool hasAlpha      = m_renderCtx.getRenderTarget().getPixelFormat().alphaBits > 0;
    tcu::TextureLevel rendered(tcu::TextureFormat(hasAlpha ? tcu::TextureFormat::RGBA : tcu::TextureFormat::RGB,
                                                  tcu::TextureFormat::UNORM_INT8),
                               viewportWidth, viewportHeight);
    tcu::TextureLevel reference(tcu::TextureFormat(hasAlpha ? tcu::TextureFormat::RGBA : tcu::TextureFormat::RGB,
                                                   tcu::TextureFormat::UNORM_INT8),
                                viewportWidth, viewportHeight);

    // Reference program executor.
    rsg::ProgramExecutor executor(reference.getAccess(), m_gridWidth, m_gridHeight);

    GLU_CHECK_CALL(glUseProgram(program.getProgram()));

    // Set up attributes
    for (vector<VertexArray>::const_iterator attribIter = m_vertexArrays.begin(); attribIter != m_vertexArrays.end();
         attribIter++)
    {
        GLint location = glGetAttribLocation(program.getProgram(), attribIter->getName());

        // Print to log.
        log << tcu::TestLog::Message << "attribute[" << location << "]: " << attribIter->getName() << " = "
            << attribIter->getValueRange() << tcu::TestLog::EndMessage;

        if (location >= 0)
        {
            glVertexAttribPointer(location, attribIter->getNumComponents(), GL_FLOAT, GL_FALSE, 0,
                                  &attribIter->getVertices()[0]);
            glEnableVertexAttribArray(location);
        }
    }
    GLU_CHECK_MSG("After attribute setup");

    // Uniforms
    for (vector<rsg::VariableValue>::const_iterator uniformIter = m_uniforms.begin(); uniformIter != m_uniforms.end();
         uniformIter++)
    {
        GLint location = glGetUniformLocation(program.getProgram(), uniformIter->getVariable()->getName());

        log << tcu::TestLog::Message << "uniform[" << location << "]: " << uniformIter->getVariable()->getName()
            << " = " << uniformIter->getValue() << tcu::TestLog::EndMessage;

        if (location >= 0)
            setUniformValue(location, uniformIter->getValue());
    }
    GLU_CHECK_MSG("After uniform setup");

    // Textures
    vector<pair<int, const glu::Texture2D *>> tex2DBindings     = m_texManager.getBindings2D();
    vector<pair<int, const glu::TextureCube *>> texCubeBindings = m_texManager.getBindingsCube();

    for (vector<pair<int, const glu::Texture2D *>>::const_iterator i = tex2DBindings.begin(); i != tex2DBindings.end();
         i++)
    {
        int unitNdx                   = i->first;
        const glu::Texture2D *texture = i->second;

        glActiveTexture(GL_TEXTURE0 + unitNdx);
        glBindTexture(GL_TEXTURE_2D, texture->getGLTexture());

        executor.setTexture(unitNdx, &texture->getRefTexture(),
                            glu::mapGLSampler(TEXTURE_WRAP_S, TEXTURE_WRAP_T, TEXTURE_MIN_FILTER, TEXTURE_MAG_FILTER));
    }
    GLU_CHECK_MSG("After 2D texture setup");

    for (vector<pair<int, const glu::TextureCube *>>::const_iterator i = texCubeBindings.begin();
         i != texCubeBindings.end(); i++)
    {
        int unitNdx                     = i->first;
        const glu::TextureCube *texture = i->second;

        glActiveTexture(GL_TEXTURE0 + unitNdx);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture->getGLTexture());

        executor.setTexture(unitNdx, &texture->getRefTexture(),
                            glu::mapGLSampler(TEXTURE_WRAP_S, TEXTURE_WRAP_T, TEXTURE_MIN_FILTER, TEXTURE_MAG_FILTER));
    }
    GLU_CHECK_MSG("After cubemap setup");

    // Draw and read
    glViewport(viewportX, viewportY, viewportWidth, viewportHeight);
    glDrawElements(GL_TRIANGLES, (GLsizei)m_indices.size(), GL_UNSIGNED_SHORT, &m_indices[0]);
    glFlush();
    GLU_CHECK_MSG("Draw");

    // Render reference while GPU is doing work
    executor.execute(m_vertexShader, m_fragmentShader, m_uniforms);

    if (rendered.getFormat().order != tcu::TextureFormat::RGBA ||
        rendered.getFormat().type != tcu::TextureFormat::UNORM_INT8)
    {
        // Read as GL_RGBA8
        tcu::TextureLevel readBuf(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8),
                                  rendered.getWidth(), rendered.getHeight());
        glu::readPixels(m_renderCtx, viewportX, viewportY, readBuf.getAccess());
        GLU_CHECK_MSG("Read pixels");
        tcu::copy(rendered, readBuf);
    }
    else
        glu::readPixels(m_renderCtx, viewportX, viewportY, rendered.getAccess());

    // Compare
    {
        float threshold = 0.02f;
        bool imagesOk   = tcu::fuzzyCompare(log, "Result", "Result images", reference.getAccess(), rendered.getAccess(),
                                            threshold, tcu::COMPARE_LOG_RESULT);

        if (imagesOk)
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
    }

    return STOP;
}

} // namespace gls
} // namespace deqp
