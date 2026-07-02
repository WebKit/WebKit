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
 * \brief Fragment operation test utilities.
 *//*--------------------------------------------------------------------*/

#include "glsFragmentOpUtil.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"
#include "gluDrawUtil.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"

namespace deqp
{
namespace gls
{
namespace FragmentOpUtil
{

template <typename T>
inline T triQuadInterpolate(const T values[4], float xFactor, float yFactor)
{
    if (xFactor + yFactor < 1.0f)
        return values[0] + (values[2] - values[0]) * xFactor + (values[1] - values[0]) * yFactor;
    else
        return values[3] + (values[1] - values[3]) * (1.0f - xFactor) + (values[2] - values[3]) * (1.0f - yFactor);
}

// GLSL ES 1.0 shaders
static const char *s_glsl1VertSrc = "attribute highp vec4 a_position;\n"
                                    "attribute mediump vec4 a_color;\n"
                                    "varying mediump vec4 v_color;\n"
                                    "void main()\n"
                                    "{\n"
                                    "    gl_Position = a_position;\n"
                                    "    v_color = a_color;\n"
                                    "}\n";
static const char *s_glsl1FragSrc = "varying mediump vec4 v_color;\n"
                                    "void main()\n"
                                    "{\n"
                                    "    gl_FragColor = v_color;\n"
                                    "}\n";

// GLSL ES 3.0 shaders
static const char *s_glsl3VertSrc = "#version 300 es\n"
                                    "in highp vec4 a_position;\n"
                                    "in mediump vec4 a_color;\n"
                                    "out mediump vec4 v_color;\n"
                                    "void main()\n"
                                    "{\n"
                                    "    gl_Position = a_position;\n"
                                    "    v_color = a_color;\n"
                                    "}\n";
static const char *s_glsl3FragSrc = "#version 300 es\n"
                                    "in mediump vec4 v_color;\n"
                                    "layout(location = 0) out mediump vec4 o_color;\n"
                                    "void main()\n"
                                    "{\n"
                                    "    o_color = v_color;\n"
                                    "}\n";

// GLSL 3.3 shaders
static const char *s_glsl33VertSrc = "#version 330 core\n"
                                     "in vec4 a_position;\n"
                                     "in vec4 a_color;\n"
                                     "in vec4 a_color1;\n"
                                     "out vec4 v_color;\n"
                                     "out vec4 v_color1;\n"
                                     "void main()\n"
                                     "{\n"
                                     "    gl_Position = a_position;\n"
                                     "    v_color = a_color;\n"
                                     "    v_color1 = a_color1;\n"
                                     "}\n";
static const char *s_glsl33FragSrc = "#version 330 core\n"
                                     "in vec4 v_color;\n"
                                     "in vec4 v_color1;\n"
                                     "layout(location = 0, index = 0) out vec4 o_color;\n"
                                     "layout(location = 0, index = 1) out vec4 o_color1;\n"
                                     "void main()\n"
                                     "{\n"
                                     "    o_color  = v_color;\n"
                                     "    o_color1 = v_color1;\n"
                                     "}\n";

static const char *getVertSrc(glu::GLSLVersion glslVersion)
{
    if (glslVersion == glu::GLSL_VERSION_100_ES)
        return s_glsl1VertSrc;
    else if (glslVersion == glu::GLSL_VERSION_300_ES)
        return s_glsl3VertSrc;
    else if (glslVersion == glu::GLSL_VERSION_330)
        return s_glsl33VertSrc;

    DE_ASSERT(false);
    return 0;
}

static const char *getFragSrc(glu::GLSLVersion glslVersion)
{
    if (glslVersion == glu::GLSL_VERSION_100_ES)
        return s_glsl1FragSrc;
    else if (glslVersion == glu::GLSL_VERSION_300_ES)
        return s_glsl3FragSrc;
    else if (glslVersion == glu::GLSL_VERSION_330)
        return s_glsl33FragSrc;

    DE_ASSERT(false);
    return 0;
}

QuadRenderer::QuadRenderer(const glu::RenderContext &context, glu::GLSLVersion glslVersion)
    : m_context(context)
    , m_program(nullptr)
    , m_positionLoc(0)
    , m_colorLoc(-1)
    , m_color1Loc(-1)
    , m_blendFuncExt(!glu::glslVersionIsES(glslVersion) && (glslVersion >= glu::GLSL_VERSION_330))
{
    DE_ASSERT(glslVersion == glu::GLSL_VERSION_100_ES || glslVersion == glu::GLSL_VERSION_300_ES ||
              glslVersion == glu::GLSL_VERSION_330);

    const glw::Functions &gl = context.getFunctions();
    const char *vertSrc      = getVertSrc(glslVersion);
    const char *fragSrc      = getFragSrc(glslVersion);

    m_program = new glu::ShaderProgram(m_context, glu::makeVtxFragSources(vertSrc, fragSrc));
    if (!m_program->isOk())
    {
        delete m_program;
        throw tcu::TestError("Failed to compile program", nullptr, __FILE__, __LINE__);
    }

    m_positionLoc = gl.getAttribLocation(m_program->getProgram(), "a_position");
    m_colorLoc    = gl.getAttribLocation(m_program->getProgram(), "a_color");

    if (m_blendFuncExt)
        m_color1Loc = gl.getAttribLocation(m_program->getProgram(), "a_color1");

    if (m_positionLoc < 0 || m_colorLoc < 0 || (m_blendFuncExt && m_color1Loc < 0))
    {
        delete m_program;
        throw tcu::TestError("Invalid attribute locations", nullptr, __FILE__, __LINE__);
    }
}

QuadRenderer::~QuadRenderer(void)
{
    delete m_program;
}

void QuadRenderer::render(const Quad &quad) const
{
    const float position[]  = {quad.posA.x(), quad.posA.y(), quad.depth[0], 1.0f,          quad.posA.x(), quad.posB.y(),
                               quad.depth[1], 1.0f,          quad.posB.x(), quad.posA.y(), quad.depth[2], 1.0f,
                               quad.posB.x(), quad.posB.y(), quad.depth[3], 1.0f};
    const uint8_t indices[] = {0, 2, 1, 1, 2, 3};

    DE_STATIC_ASSERT(sizeof(tcu::Vec4) == sizeof(float) * 4);
    DE_STATIC_ASSERT(sizeof(quad.color) == sizeof(float) * 4 * 4);
    DE_STATIC_ASSERT(sizeof(quad.color1) == sizeof(float) * 4 * 4);

    std::vector<glu::VertexArrayBinding> vertexArrays;

    vertexArrays.push_back(glu::va::Float(m_positionLoc, 4, 4, 0, &position[0]));
    vertexArrays.push_back(glu::va::Float(m_colorLoc, 4, 4, 0, (const float *)&quad.color[0]));

    if (m_blendFuncExt)
        vertexArrays.push_back(glu::va::Float(m_color1Loc, 4, 4, 0, (const float *)&quad.color1[0]));

    m_context.getFunctions().useProgram(m_program->getProgram());
    glu::draw(m_context, m_program->getProgram(), (int)vertexArrays.size(), &vertexArrays[0],
              glu::pr::Triangles(DE_LENGTH_OF_ARRAY(indices), &indices[0]));
}

ReferenceQuadRenderer::ReferenceQuadRenderer(void) : m_fragmentBufferSize(0)
{
    for (int i = 0; i < DE_LENGTH_OF_ARRAY(m_fragmentDepths); i++)
        m_fragmentDepths[i] = 0.0f;
}

void ReferenceQuadRenderer::flushFragmentBuffer(const rr::MultisamplePixelBufferAccess &colorBuffer,
                                                const rr::MultisamplePixelBufferAccess &depthBuffer,
                                                const rr::MultisamplePixelBufferAccess &stencilBuffer,
                                                rr::FaceType faceType, const rr::FragmentOperationState &state)
{
    m_fragmentProcessor.render(colorBuffer, depthBuffer, stencilBuffer, &m_fragmentBuffer[0], m_fragmentBufferSize,
                               faceType, state);
    m_fragmentBufferSize = 0;
}

void ReferenceQuadRenderer::render(const tcu::PixelBufferAccess &colorBuffer, const tcu::PixelBufferAccess &depthBuffer,
                                   const tcu::PixelBufferAccess &stencilBuffer, const IntegerQuad &quad,
                                   const rr::FragmentOperationState &state)
{
    bool flipX            = quad.posA.x() > quad.posB.x();
    bool flipY            = quad.posA.y() > quad.posB.y();
    rr::FaceType faceType = flipX == flipY ? rr::FACETYPE_FRONT : rr::FACETYPE_BACK;
    int xFirst            = flipX ? quad.posB.x() : quad.posA.x();
    int xLast             = flipX ? quad.posA.x() : quad.posB.x();
    int yFirst            = flipY ? quad.posB.y() : quad.posA.y();
    int yLast             = flipY ? quad.posA.y() : quad.posB.y();
    float width           = (float)(xLast - xFirst + 1);
    float height          = (float)(yLast - yFirst + 1);

    for (int y = yFirst; y <= yLast; y++)
    {
        // Interpolation factor for y.
        float yRatio = (0.5f + (float)(y - yFirst)) / height;
        if (flipY)
            yRatio = 1.0f - yRatio;

        for (int x = xFirst; x <= xLast; x++)
        {
            // Interpolation factor for x.
            float xRatio = (0.5f + (float)(x - xFirst)) / width;
            if (flipX)
                xRatio = 1.0f - xRatio;

            tcu::Vec4 color  = triQuadInterpolate(quad.color, xRatio, yRatio);
            tcu::Vec4 color1 = triQuadInterpolate(quad.color1, xRatio, yRatio);
            float depth      = triQuadInterpolate(quad.depth, xRatio, yRatio);

            // Interpolated color and depth.

            DE_STATIC_ASSERT(MAX_FRAGMENT_BUFFER_SIZE == DE_LENGTH_OF_ARRAY(m_fragmentBuffer));

            if (m_fragmentBufferSize >= MAX_FRAGMENT_BUFFER_SIZE)
                flushFragmentBuffer(rr::MultisamplePixelBufferAccess::fromMultisampleAccess(colorBuffer),
                                    rr::MultisamplePixelBufferAccess::fromMultisampleAccess(depthBuffer),
                                    rr::MultisamplePixelBufferAccess::fromMultisampleAccess(stencilBuffer), faceType,
                                    state);

            m_fragmentDepths[m_fragmentBufferSize] = depth;
            m_fragmentBuffer[m_fragmentBufferSize] =
                rr::Fragment(tcu::IVec2(x, y), rr::GenericVec4(color), rr::GenericVec4(color1), 1u /* coverage mask */,
                             &m_fragmentDepths[m_fragmentBufferSize]);
            m_fragmentBufferSize++;
        }
    }

    flushFragmentBuffer(rr::MultisamplePixelBufferAccess::fromMultisampleAccess(colorBuffer),
                        rr::MultisamplePixelBufferAccess::fromMultisampleAccess(depthBuffer),
                        rr::MultisamplePixelBufferAccess::fromMultisampleAccess(stencilBuffer), faceType, state);
}

tcu::PixelBufferAccess getMultisampleAccess(const tcu::PixelBufferAccess &original)
{
    return tcu::PixelBufferAccess(original.getFormat(), 1, original.getWidth(), original.getHeight(),
                                  original.getFormat().getPixelSize(), original.getRowPitch(), original.getDataPtr());
}

tcu::ConstPixelBufferAccess getMultisampleAccess(const tcu::ConstPixelBufferAccess &original)
{
    return tcu::ConstPixelBufferAccess(original.getFormat(), 1, original.getWidth(), original.getHeight(),
                                       original.getFormat().getPixelSize(), original.getRowPitch(),
                                       original.getDataPtr());
}

} // namespace FragmentOpUtil
} // namespace gls
} // namespace deqp
