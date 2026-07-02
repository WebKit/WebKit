#ifndef _GLSRANDOMSHADERCASE_HPP
#define _GLSRANDOMSHADERCASE_HPP
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

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "gluRenderContext.hpp"
#include "rsgParameters.hpp"
#include "tcuSurface.hpp"
#include "rsgShader.hpp"
#include "rsgVariableValue.hpp"
#include "gluTexture.hpp"

#include <string>
#include <vector>
#include <map>

namespace deqp
{
namespace gls
{

class VertexArray
{
public:
    VertexArray(const rsg::ShaderInput *input, int numVertices);
    ~VertexArray(void)
    {
    }

    const std::vector<float> &getVertices(void) const
    {
        return m_vertices;
    }
    std::vector<float> &getVertices(void)
    {
        return m_vertices;
    }
    const char *getName(void) const
    {
        return m_input->getVariable()->getName();
    }
    int getNumComponents(void) const
    {
        return m_input->getVariable()->getType().getNumElements();
    }
    rsg::ConstValueRangeAccess getValueRange(void) const
    {
        return m_input->getValueRange();
    }

private:
    const rsg::ShaderInput *m_input;
    std::vector<float> m_vertices;
};

class TextureManager
{
public:
    TextureManager(void);
    ~TextureManager(void);

    void bindTexture(int unit, const glu::Texture2D *tex2D);
    void bindTexture(int unit, const glu::TextureCube *texCube);

    std::vector<std::pair<int, const glu::Texture2D *>> getBindings2D(void) const;
    std::vector<std::pair<int, const glu::TextureCube *>> getBindingsCube(void) const;

private:
    std::map<int, const glu::Texture2D *> m_tex2D;
    std::map<int, const glu::TextureCube *> m_texCube;
};

class RandomShaderCase : public tcu::TestCase
{
public:
    RandomShaderCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                     const char *description, const rsg::ProgramParameters &params);
    virtual ~RandomShaderCase(void);

    virtual void init(void);
    virtual void deinit(void);
    virtual IterateResult iterate(void);

private:
    void checkShaderLimits(const rsg::Shader &shader) const;
    void checkProgramLimits(const rsg::Shader &vtxShader, const rsg::Shader &frgShader) const;

protected:
    glu::RenderContext &m_renderCtx;

    // \todo [2011-12-21 pyry] Multiple textures!
    const glu::Texture2D *getTex2D(void);
    const glu::TextureCube *getTexCube(void);

    rsg::ProgramParameters m_parameters;
    int m_gridWidth;
    int m_gridHeight;

    rsg::Shader m_vertexShader;
    rsg::Shader m_fragmentShader;
    std::vector<rsg::VariableValue> m_uniforms;

    std::vector<VertexArray> m_vertexArrays;
    std::vector<uint16_t> m_indices;

    TextureManager m_texManager;
    glu::Texture2D *m_tex2D;
    glu::TextureCube *m_texCube;
};

} // namespace gls
} // namespace deqp

#endif // _GLSRANDOMSHADERCASE_HPP
