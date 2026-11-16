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
 * \brief State change performance tests.
 *//*--------------------------------------------------------------------*/

#include "es3pStateChangeTests.hpp"
#include "glsStateChangePerfTestCases.hpp"
#include "gluShaderProgram.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"

namespace deqp
{
namespace gles3
{
namespace Performance
{

using namespace glw; // GL types

namespace
{

enum
{
    VIEWPORT_WIDTH  = 24,
    VIEWPORT_HEIGHT = 24
};

class StateChangeCase : public gls::StateChangePerformanceCase
{
public:
    StateChangeCase(Context &context, int drawCallCount, int triangleCount, bool drawArrays, bool useIndexBuffer,
                    const char *name, const char *description);
    ~StateChangeCase(void);

protected:
    virtual void renderTest(const glw::Functions &gl);
    virtual void renderReference(const glw::Functions &gl);

    virtual void changeState(const glw::Functions &gl, int stateId) = 0;
};

StateChangeCase::StateChangeCase(Context &context, int drawCallCount, int triangleCount, bool drawArrays,
                                 bool useIndexBuffer, const char *name, const char *description)
    : gls::StateChangePerformanceCase(context.getTestContext(), context.getRenderContext(), name, description,
                                      (useIndexBuffer ? DRAWTYPE_INDEXED_BUFFER :
                                       drawArrays     ? DRAWTYPE_NOT_INDEXED :
                                                        DRAWTYPE_INDEXED_USER_PTR),
                                      drawCallCount, triangleCount)
{
    DE_ASSERT(!useIndexBuffer || !drawArrays);
}

StateChangeCase::~StateChangeCase(void)
{
}

void StateChangeCase::renderTest(const glw::Functions &gl)
{
    for (int callNdx = 0; callNdx < m_callCount; callNdx++)
    {
        changeState(gl, 0);
        callDraw(gl);

        changeState(gl, 1);
        callDraw(gl);
    }
}

void StateChangeCase::renderReference(const glw::Functions &gl)
{
    changeState(gl, 0);

    for (int callNdx = 0; callNdx < m_callCount; callNdx++)
        callDraw(gl);

    changeState(gl, 1);

    for (int callNdx = 0; callNdx < m_callCount; callNdx++)
        callDraw(gl);
}

} // namespace

StateChangeTests::StateChangeTests(Context &context)
    : TestCaseGroup(context, "state_change_draw", "Test state change perfomance with draw calls.")
{
}

StateChangeTests::~StateChangeTests(void)
{
}

#define MACRO_BLOCK(...) __VA_ARGS__

#define ADD_TESTCASE(NAME, DESC, DRAWARRAYS, INDEXBUFFER, INIT_FUNC, CHANGE_FUNC)                            \
    do                                                                                                       \
    {                                                                                                        \
        class StateChangeCase_##NAME : public StateChangeCase                                                \
        {                                                                                                    \
        public:                                                                                              \
            StateChangeCase_##NAME(Context &context, int drawCallCount, int triangleCount, const char *name, \
                                   const char *description)                                                  \
                : StateChangeCase(context, drawCallCount, triangleCount, (DRAWARRAYS), (INDEXBUFFER), name,  \
                                  description)                                                               \
            {                                                                                                \
            }                                                                                                \
            virtual void setupInitialState(const glw::Functions &gl)                                         \
            {                                                                                                \
                INIT_FUNC                                                                                    \
            }                                                                                                \
            virtual void changeState(const glw::Functions &gl, int stateId)                                  \
            {                                                                                                \
                CHANGE_FUNC                                                                                  \
            }                                                                                                \
        };                                                                                                   \
        manySmallCallsGroup->addChild(new StateChangeCase_##NAME(m_context, 1000, 2, #NAME, (DESC)));        \
        fewBigCallsGroup->addChild(new StateChangeCase_##NAME(m_context, 10, 200, #NAME, (DESC)));           \
    } while (0);

void StateChangeTests::init(void)
{
    tcu::TestCaseGroup *const manySmallCallsGroup =
        new tcu::TestCaseGroup(m_testCtx, "many_small_calls", "1000 calls, 2 triangles in each");
    tcu::TestCaseGroup *const fewBigCallsGroup =
        new tcu::TestCaseGroup(m_testCtx, "few_big_calls", "10 calls, 200 triangles in each");

    addChild(manySmallCallsGroup);
    addChild(fewBigCallsGroup);

    ADD_TESTCASE(blend, "Enable/Disable blending.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.enable(GL_BLEND);
                     else if (stateId == 1)
                         gl.disable(GL_BLEND);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(depth_test, "Enable/Disable depth test.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");

                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");

                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");

                     gl.depthFunc(GL_LEQUAL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glDepthFunc()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.enable(GL_DEPTH_TEST);
                     else if (stateId == 1)
                         gl.disable(GL_DEPTH_TEST);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(stencil_test, "Enable/Disable stencil test.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");

                     gl.stencilFunc(GL_LEQUAL, 0, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glStencilFunc()");

                     gl.stencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glStencilOp()");

                     gl.clearStencil(0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glClearStencil()");
                     gl.clear(GL_STENCIL_BUFFER_BIT);

                     GLU_EXPECT_NO_ERROR(gl.getError(), "glClear()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.enable(GL_STENCIL_TEST);
                     else if (stateId == 1)
                         gl.disable(GL_STENCIL_TEST);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(scissor_test, "Enable/Disable scissor test.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");

                     gl.scissor(2, 3, 12, 13);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glScissor()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.enable(GL_SCISSOR_TEST);
                     else if (stateId == 1)
                         gl.disable(GL_SCISSOR_TEST);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(dither, "Enable/Disable dithering.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.enable(GL_DITHER);
                     else if (stateId == 1)
                         gl.disable(GL_DITHER);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(culling, "Enable/Disable culling.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");

                     gl.frontFace(GL_CW);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glFrontFace()");

                     gl.cullFace(GL_FRONT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glCullFace()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.enable(GL_CULL_FACE);
                     else if (stateId == 1)
                         gl.disable(GL_CULL_FACE);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(rasterizer_discard, "Enable/Disable RASTERIZER_DISCARD.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.enable(GL_RASTERIZER_DISCARD);
                     else if (stateId == 1)
                         gl.disable(GL_RASTERIZER_DISCARD);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(primitive_restart_fixed_index, "Enable/Disable PRIMITIVE_RESTART_FIXED_INDEX.", true, false,
                 MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.enable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
                     else if (stateId == 1)
                         gl.disable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(depth_func, "Change depth func.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");

                     gl.enable(GL_DEPTH_TEST);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnable()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.depthFunc(GL_GEQUAL);
                     else if (stateId == 1)
                         gl.depthFunc(GL_LEQUAL);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(depth_mask, "Toggle depth mask.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");

                     gl.enable(GL_DEPTH_TEST);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnable()");

                     gl.depthFunc(GL_LEQUAL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glDepthFunc()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.depthMask(GL_FALSE);
                     else if (stateId == 1)
                         gl.depthMask(GL_TRUE);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(depth_rangef, "Change depth range.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.depthRangef(0.0f, 1.0f);
                     else if (stateId == 1)
                         gl.depthRangef(0.25f, 0.75f);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(blend_equation, "Change blend equation.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");

                     gl.enable(GL_BLEND);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnable()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.blendEquation(GL_FUNC_SUBTRACT);
                     else if (stateId == 1)
                         gl.blendEquation(GL_FUNC_ADD);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(blend_func, "Change blend function.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");

                     gl.enable(GL_BLEND);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnable()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                     else if (stateId == 1)
                         gl.blendFunc(GL_ONE, GL_ONE);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(polygon_offset, "Change polygon offset.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");

                     gl.enable(GL_POLYGON_OFFSET_FILL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnable()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.polygonOffset(0.0f, 0.0f);
                     else if (stateId == 1)
                         gl.polygonOffset(0.1f, 0.1f);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(sample_coverage, "Sample coverage.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.sampleCoverage(0.25f, GL_TRUE);
                     else if (stateId == 1)
                         gl.sampleCoverage(0.75f, GL_FALSE);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(viewport, "Change viewport.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.viewport(10, 11, 5, 6);
                     else if (stateId == 1)
                         gl.viewport(2, 3, 17, 14);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(scissor, "Change scissor box.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");

                     gl.enable(GL_SCISSOR_TEST);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnable()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.scissor(17, 13, 5, 8);
                     else if (stateId == 1)
                         gl.scissor(7, 3, 13, 13);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(color_mask, "Change color mask.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.colorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
                     else if (stateId == 1)
                         gl.colorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(cull_face, "Change culling mode.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");

                     gl.enable(GL_CULL_FACE);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnable()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.cullFace(GL_FRONT);
                     else if (stateId == 1)
                         gl.cullFace(GL_BACK);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(front_face, "Change front face.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");

                     gl.enable(GL_CULL_FACE);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnable()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.frontFace(GL_CCW);
                     else if (stateId == 1)
                         gl.frontFace(GL_CW);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(stencil_mask, "Change stencil mask.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");

                     gl.enable(GL_STENCIL_TEST);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnable()");

                     gl.stencilFunc(GL_LEQUAL, 0, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glStencilFunc()");

                     gl.stencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glStencilOp()");

                     gl.clearStencil(0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glClearStencil()");
                     gl.clear(GL_STENCIL_BUFFER_BIT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glClear()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.stencilMask(0xDD);
                     else if (stateId == 1)
                         gl.stencilMask(~0xDD);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(stencil_func, "Change stencil func.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");

                     gl.enable(GL_STENCIL_TEST);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnable()");

                     gl.stencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glStencilOp()");
                     gl.clearStencil(0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glClearStencil()");
                     gl.clear(GL_STENCIL_BUFFER_BIT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glClear()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.stencilFunc(GL_LEQUAL, 0, 0xFF);
                     else if (stateId == 1)
                         gl.stencilFunc(GL_GEQUAL, 0, 0x00);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(stencil_op, "Change stencil op.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");

                     gl.enable(GL_STENCIL_TEST);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnable()");

                     gl.stencilFunc(GL_LEQUAL, 0, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glStencilFunc()");

                     gl.clearStencil(0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glClearStencil()");

                     gl.clear(GL_STENCIL_BUFFER_BIT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glClear()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.stencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
                     else if (stateId == 1)
                         gl.stencilOp(GL_REPLACE, GL_KEEP, GL_KEEP);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(bind_array_buffer, "Change array buffer and refresh vertex attrib pointer.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(2);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.bindAttribLocation(m_programs[0]->getProgram(), 0, "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindAttribLocation()");
                     gl.linkProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glLinkProgram()");

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");

                     gl.enableVertexAttribArray(0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                     {
                         gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                         gl.vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     }
                     else if (stateId == 1)
                     {
                         gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[1]);
                         gl.vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     }
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(element_array_buffer, "Change element array buffer.", false, true, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireIndexBuffers(2);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");

                     gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffers[0]);
                     else if (stateId == 1)
                         gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffers[1]);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(bind_texture, "Change texture binding.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(2);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     else if (stateId == 1)
                         gl.bindTexture(GL_TEXTURE_2D, m_textures[1]);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(use_program, "Change used program.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(2);

                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     {
                         GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                         GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");
                         gl.useProgram(m_programs[0]->getProgram());
                         GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                         gl.uniform1i(samplerLoc, 0);
                         GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");
                     }

                     {
                         GLint samplerLoc = gl.getUniformLocation(m_programs[1]->getProgram(), "u_sampler");
                         GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");
                         gl.useProgram(m_programs[1]->getProgram());
                         GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                         gl.uniform1i(samplerLoc, 0);
                         GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");
                     }

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.useProgram(m_programs[0]->getProgram());
                     else if (stateId == 1)
                         gl.useProgram(m_programs[1]->getProgram());
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(tex_parameter_min_filter, "Change texture parameter min filter.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                     else if (stateId == 1)
                         gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(tex_parameter_mag_filter, "Change texture parameter mag filter.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                     else if (stateId == 1)
                         gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(tex_parameter_wrap, "Change texture parameter wrap filter.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                     else if (stateId == 1)
                         gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(bind_framebuffer, "Change framebuffer.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requireFramebuffers(2);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");

                     gl.bindFramebuffer(GL_FRAMEBUFFER, m_framebuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindFramebuffer()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.bindFramebuffer(GL_FRAMEBUFFER, m_framebuffers[0]);
                     else if (stateId == 1)
                         gl.bindFramebuffer(GL_FRAMEBUFFER, m_framebuffers[1]);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(blend_color, "Change blend color.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");

                     gl.enable(GL_BLEND);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnable()");

                     gl.blendFunc(GL_CONSTANT_COLOR, GL_CONSTANT_COLOR);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBlendFunc()");
                 }),
                 MACRO_BLOCK({
                     if (stateId == 0)
                         gl.blendColor(0.25f, 0.25f, 0.25f, 0.25f);
                     else if (stateId == 1)
                         gl.blendColor(0.75f, 0.75f, 0.75f, 0.75f);
                     else
                         DE_ASSERT(false);
                 }))

    ADD_TESTCASE(sampler, "Change sampler binding.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(1);
                     requireTextures(1);
                     requirePrograms(1);
                     requireSamplers(2);

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
                     GLint coordLoc = gl.getAttribLocation(m_programs[0]->getProgram(), "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation()");

                     gl.enableVertexAttribArray(coordLoc);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                     gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                     gl.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");

                     for (int ndx = 0; ndx < 2; ndx++)
                     {
                         gl.bindSampler(0, m_samplers[ndx]);
                         gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ndx == 0 ? GL_NEAREST : GL_LINEAR);
                         gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, ndx == 0 ? GL_LINEAR : GL_NEAREST);
                         GLU_EXPECT_NO_ERROR(gl.getError(), "Sampler setup");
                     }
                 }),
                 MACRO_BLOCK({
                     DE_ASSERT(de::inBounds(stateId, 0, 2));
                     gl.bindSampler(0, m_samplers[stateId]);
                 }))

    ADD_TESTCASE(bind_vertex_array, "Change vertex array binding.", true, false, MACRO_BLOCK({
                     requireCoordBuffers(2);
                     requireTextures(1);
                     requirePrograms(1);
                     requireVertexArrays(2);

                     gl.bindAttribLocation(m_programs[0]->getProgram(), 0, "a_coord");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindAttribLocation()");
                     gl.linkProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glLinkProgram()");

                     gl.useProgram(m_programs[0]->getProgram());
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");

                     for (int ndx = 0; ndx < 2; ndx++)
                     {
                         gl.bindVertexArray(m_vertexArrays[ndx]);
                         GLU_EXPECT_NO_ERROR(gl.getError(), "glBindVertexArray()");
                         gl.enableVertexAttribArray(0);
                         GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray()");
                         gl.bindBuffer(GL_ARRAY_BUFFER, m_coordBuffers[ndx]);
                         GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer()");
                         gl.vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                         GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer()");
                     }

                     GLint samplerLoc = gl.getUniformLocation(m_programs[0]->getProgram(), "u_sampler");
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glGetUniformLocation()");

                     gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture()");

                     gl.uniform1i(samplerLoc, 0);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i()");

                     gl.viewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
                     GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport()");
                 }),
                 MACRO_BLOCK({
                     DE_ASSERT(de::inRange(stateId, 0, 2));
                     gl.bindVertexArray(m_vertexArrays[stateId]);
                 }))
}

} // namespace Performance
} // namespace gles3
} // namespace deqp
