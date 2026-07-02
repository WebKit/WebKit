#ifndef _ES2PTEXTURECASES_HPP
#define _ES2PTEXTURECASES_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
 * -------------------------------------------------
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
 * \brief Texture format performance tests.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tes2TestCase.hpp"
#include "glsShaderPerformanceCase.hpp"
#include "tcuMatrix.hpp"
#include "gluTexture.hpp"

namespace deqp
{
namespace gles2
{
namespace Performance
{

class Texture2DRenderCase : public gls::ShaderPerformanceCase
{
public:
    Texture2DRenderCase(Context &context, const char *name, const char *description, uint32_t format, uint32_t dataType,
                        uint32_t wrapS, uint32_t wrapT, uint32_t minFilter, uint32_t magFilter,
                        const tcu::Mat3 &coordTransform, int numTextures, bool powerOfTwo);
    ~Texture2DRenderCase(void);

    void init(void);
    void deinit(void);

private:
    void setupProgram(uint32_t program);
    void setupRenderState(void);

    uint32_t m_format;
    uint32_t m_dataType;
    uint32_t m_wrapS;
    uint32_t m_wrapT;
    uint32_t m_minFilter;
    uint32_t m_magFilter;
    tcu::Mat3 m_coordTransform;
    int m_numTextures;
    bool m_powerOfTwo;
    std::vector<glu::Texture2D *> m_textures;
};

} // namespace Performance
} // namespace gles2
} // namespace deqp

#endif // _ES2PTEXTURECASES_HPP
