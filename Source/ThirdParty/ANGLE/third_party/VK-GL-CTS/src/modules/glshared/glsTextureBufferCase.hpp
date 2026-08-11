#ifndef _GLSTEXTUREBUFFERCASE_HPP
#define _GLSTEXTUREBUFFERCASE_HPP
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
 * \brief Texture buffer test case
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

namespace glu
{
class RenderContext;
class ShaderProgram;
} // namespace glu

namespace deqp
{
namespace gls
{

namespace TextureBufferCaseUtil
{

enum ModifyBits
{
    MODIFYBITS_NONE                = 0,
    MODIFYBITS_BUFFERDATA          = (0x1 << 0),
    MODIFYBITS_BUFFERSUBDATA       = (0x1 << 1),
    MODIFYBITS_MAPBUFFER_WRITE     = (0x1 << 2),
    MODIFYBITS_MAPBUFFER_READWRITE = (0x1 << 3),
};

enum RenderBits
{
    RENDERBITS_NONE                = 0,
    RENDERBITS_AS_VERTEX_ARRAY     = (0x1 << 0),
    RENDERBITS_AS_INDEX_ARRAY      = (0x1 << 1),
    RENDERBITS_AS_VERTEX_TEXTURE   = (0x1 << 2),
    RENDERBITS_AS_FRAGMENT_TEXTURE = (0x1 << 3)
};

} // namespace TextureBufferCaseUtil

class TextureBufferCase : public tcu::TestCase
{
public:
    TextureBufferCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, uint32_t format, size_t bufferSize,
                      size_t offset, size_t size, TextureBufferCaseUtil::RenderBits preRender,
                      TextureBufferCaseUtil::ModifyBits modify, TextureBufferCaseUtil::RenderBits postRender,
                      const char *name, const char *description);

    ~TextureBufferCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    glu::RenderContext &m_renderCtx;
    const uint32_t m_format;
    const size_t m_bufferSize;
    const size_t m_offset;
    const size_t m_size;
    const TextureBufferCaseUtil::RenderBits m_preRender;
    const TextureBufferCaseUtil::ModifyBits m_modify;
    const TextureBufferCaseUtil::RenderBits m_postRender;

    glu::ShaderProgram *m_preRenderProgram;
    glu::ShaderProgram *m_postRenderProgram;
};

} // namespace gls
} // namespace deqp

#endif // _GLSTEXTUREBUFFERCASE_HPP
