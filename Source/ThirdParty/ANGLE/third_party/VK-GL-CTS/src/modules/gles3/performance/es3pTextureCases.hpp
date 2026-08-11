#ifndef _ES3PTEXTURECASES_HPP
#define _ES3PTEXTURECASES_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
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
#include "tes3TestCase.hpp"
#include "glsShaderPerformanceCase.hpp"
#include "tcuMatrix.hpp"
#include "gluTexture.hpp"

namespace deqp
{
namespace gles3
{
namespace Performance
{

class Texture2DRenderCase : public gls::ShaderPerformanceCase
{
public:
    Texture2DRenderCase(Context &context, const char *name, const char *description, uint32_t internalFormat,
                        uint32_t wrapS, uint32_t wrapT, uint32_t minFilter, uint32_t magFilter,
                        const tcu::Mat3 &coordTransform, int numTextures, bool powerOfTwo);
    ~Texture2DRenderCase(void);

    void init(void);
    void deinit(void);

private:
    void setupProgram(uint32_t program);
    void setupRenderState(void);

    const uint32_t m_internalFormat;
    const uint32_t m_wrapS;
    const uint32_t m_wrapT;
    const uint32_t m_minFilter;
    const uint32_t m_magFilter;
    const tcu::Mat3 m_coordTransform;
    const int m_numTextures;
    const bool m_powerOfTwo;

    std::vector<glu::Texture2D *> m_textures;
};

} // namespace Performance
} // namespace gles3
} // namespace deqp

#endif // _ES3PTEXTURECASES_HPP
