#ifndef _GLSFRAGMENTOPUTIL_HPP
#define _GLSFRAGMENTOPUTIL_HPP
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

#include "tcuDefs.hpp"
#include "gluShaderUtil.hpp"
#include "tcuVector.hpp"
#include "tcuTexture.hpp"
#include "rrFragmentOperations.hpp"

namespace glu
{
class ShaderProgram;
class RenderContext;
} // namespace glu

namespace deqp
{
namespace gls
{
namespace FragmentOpUtil
{

struct Quad
{
    tcu::Vec2 posA;
    tcu::Vec2 posB;

    // Normalized device coordinates (range [-1, 1]).
    // In order (A.x, A.y), (A.x, B.y), (B.x, A.y), (B.x, B.y)
    tcu::Vec4 color[4];
    tcu::Vec4 color1[4];
    float depth[4];

    Quad(void) : posA(-1.0f, -1.0f), posB(1.0f, 1.0f)
    {
        for (int i = 0; i < DE_LENGTH_OF_ARRAY(depth); i++)
            depth[i] = 0.0f;
    }
};

class QuadRenderer
{
public:
    QuadRenderer(const glu::RenderContext &context, glu::GLSLVersion glslVersion);
    ~QuadRenderer(void);

    void render(const Quad &quad) const;

private:
    QuadRenderer(const QuadRenderer &other);            // Not allowed!
    QuadRenderer &operator=(const QuadRenderer &other); // Not allowed!

    const glu::RenderContext &m_context;
    glu::ShaderProgram *m_program;
    int m_positionLoc;
    int m_colorLoc;
    int m_color1Loc;
    const bool m_blendFuncExt;
};

struct IntegerQuad
{
    tcu::IVec2 posA;
    tcu::IVec2 posB;

    // Viewport coordinates (depth in range [0, 1]).
    // In order (A.x, A.y), (A.x, B.y), (B.x, A.y), (B.x, B.y)
    tcu::Vec4 color[4];
    tcu::Vec4 color1[4];
    float depth[4];

    IntegerQuad(int windowWidth, int windowHeight) : posA(0, 0), posB(windowWidth - 1, windowHeight - 1)
    {
        for (int i = 0; i < DE_LENGTH_OF_ARRAY(depth); i++)
            depth[i] = 0.0f;
    }

    IntegerQuad(void) : posA(0, 0), posB(1, 1)
    {
        for (int i = 0; i < DE_LENGTH_OF_ARRAY(depth); i++)
            depth[i] = 0.0f;
    }
};

class ReferenceQuadRenderer
{
public:
    ReferenceQuadRenderer(void);

    void render(const tcu::PixelBufferAccess &colorBuffer, const tcu::PixelBufferAccess &depthBuffer,
                const tcu::PixelBufferAccess &stencilBuffer, const IntegerQuad &quad,
                const rr::FragmentOperationState &state);

private:
    enum
    {
        MAX_FRAGMENT_BUFFER_SIZE = 1024
    };

    void flushFragmentBuffer(const rr::MultisamplePixelBufferAccess &colorBuffer,
                             const rr::MultisamplePixelBufferAccess &depthBuffer,
                             const rr::MultisamplePixelBufferAccess &stencilBuffer, rr::FaceType faceType,
                             const rr::FragmentOperationState &state);

    rr::Fragment m_fragmentBuffer[MAX_FRAGMENT_BUFFER_SIZE];
    float m_fragmentDepths[MAX_FRAGMENT_BUFFER_SIZE];
    int m_fragmentBufferSize;

    rr::FragmentProcessor m_fragmentProcessor;
};

// These functions take a normally-indexed 2d pixel buffer and return a pixel buffer access
// that indexes the same memory area, but using the multisample indexing convention.
tcu::PixelBufferAccess getMultisampleAccess(const tcu::PixelBufferAccess &original);
tcu::ConstPixelBufferAccess getMultisampleAccess(const tcu::ConstPixelBufferAccess &original);

} // namespace FragmentOpUtil
} // namespace gls
} // namespace deqp

#endif // _GLSFRAGMENTOPUTIL_HPP
