#ifndef _GLSSTATECHANGEPERFTESTCASES_HPP
#define _GLSSTATECHANGEPERFTESTCASES_HPP
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
 * \brief State change performance tests.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

namespace glu
{
class ShaderProgram;
class RenderContext;
} // namespace glu

namespace glw
{
class Functions;
}

namespace deqp
{
namespace gls
{

class StateChangePerformanceCase : public tcu::TestCase
{
public:
    enum DrawType
    {
        DRAWTYPE_NOT_INDEXED = 0,  //!< glDrawArrays()
        DRAWTYPE_INDEXED_USER_PTR, //!< glDrawElements(), indices from user pointer.
        DRAWTYPE_INDEXED_BUFFER,   //!< glDrawElements(), indices in buffer.
    };

    StateChangePerformanceCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                               const char *description, DrawType drawType, int drawCallCount, int triangleCount);
    ~StateChangePerformanceCase(void);

    void init(void);
    void deinit(void);

    IterateResult iterate(void);

protected:
    void requireIndexBuffers(int count);
    void requireCoordBuffers(int count);
    void requirePrograms(int count);
    void requireTextures(int count);
    void requireFramebuffers(int count);
    void requireRenderbuffers(int count);
    void requireSamplers(int count);
    void requireVertexArrays(int count);

    virtual void setupInitialState(const glw::Functions &gl) = 0;
    virtual void renderTest(const glw::Functions &gl)        = 0;
    virtual void renderReference(const glw::Functions &gl)   = 0;

    void callDraw(const glw::Functions &gl);

    void logAndSetTestResult(void);

protected:
    glu::RenderContext &m_renderCtx;

    const DrawType m_drawType;
    const int m_iterationCount;
    const int m_callCount;
    const int m_triangleCount;

    std::vector<uint32_t> m_indexBuffers;
    std::vector<uint32_t> m_coordBuffers;
    std::vector<uint32_t> m_textures;
    std::vector<glu::ShaderProgram *> m_programs;
    std::vector<uint32_t> m_framebuffers;
    std::vector<uint32_t> m_renderbuffers;
    std::vector<uint32_t> m_samplers;
    std::vector<uint32_t> m_vertexArrays;

private:
    StateChangePerformanceCase(const StateChangePerformanceCase &);
    StateChangePerformanceCase &operator=(const StateChangePerformanceCase &);

    std::vector<uint16_t> m_indices;

    std::vector<uint64_t> m_interleavedResults;
    std::vector<uint64_t> m_batchedResults;
};

class StateChangeCallPerformanceCase : public tcu::TestCase
{
public:
    StateChangeCallPerformanceCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                                   const char *description);
    ~StateChangeCallPerformanceCase(void);

    IterateResult iterate(void);

    virtual void execCalls(const glw::Functions &gl, int iterNdx, int callCount) = 0;

private:
    void executeTest(void);
    void logTestCase(void);

    void logAndSetTestResult(void);

    glu::RenderContext &m_renderCtx;

    const int m_iterationCount;
    const int m_callCount;

    std::vector<uint64_t> m_results;
};

} // namespace gls
} // namespace deqp

#endif // _GLSSTATECHANGEPERFTESTCASES_HPP
