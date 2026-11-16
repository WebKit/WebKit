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
 * \brief State change call performance tests.
 *//*--------------------------------------------------------------------*/

#include "es2pStateChangeCallTests.hpp"
#include "glsStateChangePerfTestCases.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"

namespace deqp
{
namespace gles2
{
namespace Performance
{

using namespace glw;

StateChangeCallTests::StateChangeCallTests(Context &context)
    : TestCaseGroup(context, "state_change_only", "Test cost of state change calls without rendering anything")
{
}

StateChangeCallTests::~StateChangeCallTests(void)
{
}

#define ARG_LIST(...) __VA_ARGS__

#define ADD_ARG_CASE1(NAME, DESCRIPTION, FUNCNAME, TYPE0, ARGS0)                                                  \
    do                                                                                                            \
    {                                                                                                             \
        class StateChangeCallTest_##NAME : public gls::StateChangeCallPerformanceCase                             \
        {                                                                                                         \
        public:                                                                                                   \
            StateChangeCallTest_##NAME(Context &context, const char *name, const char *description)               \
                : gls::StateChangeCallPerformanceCase(context.getTestContext(), context.getRenderContext(), name, \
                                                      description)                                                \
            {                                                                                                     \
            }                                                                                                     \
            virtual void execCalls(const glw::Functions &gl, int iterNdx, int callCount)                          \
            {                                                                                                     \
                const TYPE0 args0[] = ARGS0;                                                                      \
                for (int callNdx = 0; callNdx < callCount; callNdx++)                                             \
                {                                                                                                 \
                    const int baseNdx = iterNdx * callCount + callNdx;                                            \
                    const TYPE0 arg0  = args0[baseNdx % DE_LENGTH_OF_ARRAY(args0)];                               \
                    gl.FUNCNAME(arg0);                                                                            \
                }                                                                                                 \
            }                                                                                                     \
        };                                                                                                        \
        addChild(new StateChangeCallTest_##NAME(m_context, #NAME, DESCRIPTION));                                  \
    } while (0);

#define ADD_ARG_CASE2(NAME, DESCRIPTION, FUNCNAME, TYPE0, ARGS0, TYPE1, ARGS1)                                    \
    do                                                                                                            \
    {                                                                                                             \
        class StateChangeCallTest_##NAME : public gls::StateChangeCallPerformanceCase                             \
        {                                                                                                         \
        public:                                                                                                   \
            StateChangeCallTest_##NAME(Context &context, const char *name, const char *description)               \
                : gls::StateChangeCallPerformanceCase(context.getTestContext(), context.getRenderContext(), name, \
                                                      description)                                                \
            {                                                                                                     \
            }                                                                                                     \
            virtual void execCalls(const glw::Functions &gl, int iterNdx, int callCount)                          \
            {                                                                                                     \
                const TYPE0 args0[] = ARGS0;                                                                      \
                const TYPE1 args1[] = ARGS1;                                                                      \
                for (int callNdx = 0; callNdx < callCount; callNdx++)                                             \
                {                                                                                                 \
                    const int baseNdx = iterNdx * callCount + callNdx;                                            \
                    const TYPE0 arg0  = args0[baseNdx % DE_LENGTH_OF_ARRAY(args0)];                               \
                    const TYPE1 arg1  = args1[baseNdx % DE_LENGTH_OF_ARRAY(args1)];                               \
                    gl.FUNCNAME(arg0, arg1);                                                                      \
                }                                                                                                 \
            }                                                                                                     \
        };                                                                                                        \
        addChild(new StateChangeCallTest_##NAME(m_context, #NAME, DESCRIPTION));                                  \
    } while (0);

#define ADD_ARG_CASE3(NAME, DESCRIPTION, FUNCNAME, TYPE0, ARGS0, TYPE1, ARGS1, TYPE2, ARGS2)                      \
    do                                                                                                            \
    {                                                                                                             \
        class StateChangeCallTest_##NAME : public gls::StateChangeCallPerformanceCase                             \
        {                                                                                                         \
        public:                                                                                                   \
            StateChangeCallTest_##NAME(Context &context, const char *name, const char *description)               \
                : gls::StateChangeCallPerformanceCase(context.getTestContext(), context.getRenderContext(), name, \
                                                      description)                                                \
            {                                                                                                     \
            }                                                                                                     \
            virtual void execCalls(const glw::Functions &gl, int iterNdx, int callCount)                          \
            {                                                                                                     \
                const TYPE0 args0[] = ARGS0;                                                                      \
                const TYPE1 args1[] = ARGS1;                                                                      \
                const TYPE2 args2[] = ARGS2;                                                                      \
                for (int callNdx = 0; callNdx < callCount; callNdx++)                                             \
                {                                                                                                 \
                    const int baseNdx = iterNdx * callCount + callNdx;                                            \
                    const TYPE0 arg0  = args0[baseNdx % DE_LENGTH_OF_ARRAY(args0)];                               \
                    const TYPE1 arg1  = args1[baseNdx % DE_LENGTH_OF_ARRAY(args1)];                               \
                    const TYPE2 arg2  = args2[baseNdx % DE_LENGTH_OF_ARRAY(args2)];                               \
                    gl.FUNCNAME(arg0, arg1, arg2);                                                                \
                }                                                                                                 \
            }                                                                                                     \
        };                                                                                                        \
        addChild(new StateChangeCallTest_##NAME(m_context, #NAME, DESCRIPTION));                                  \
    } while (0);

#define ADD_ARG_CASE4(NAME, DESCRIPTION, FUNCNAME, TYPE0, ARGS0, TYPE1, ARGS1, TYPE2, ARGS2, TYPE3, ARGS3)        \
    do                                                                                                            \
    {                                                                                                             \
        class StateChangeCallTest_##NAME : public gls::StateChangeCallPerformanceCase                             \
        {                                                                                                         \
        public:                                                                                                   \
            StateChangeCallTest_##NAME(Context &context, const char *name, const char *description)               \
                : gls::StateChangeCallPerformanceCase(context.getTestContext(), context.getRenderContext(), name, \
                                                      description)                                                \
            {                                                                                                     \
            }                                                                                                     \
            virtual void execCalls(const glw::Functions &gl, int iterNdx, int callCount)                          \
            {                                                                                                     \
                const TYPE0 args0[] = ARGS0;                                                                      \
                const TYPE1 args1[] = ARGS1;                                                                      \
                const TYPE2 args2[] = ARGS2;                                                                      \
                const TYPE3 args3[] = ARGS3;                                                                      \
                for (int callNdx = 0; callNdx < callCount; callNdx++)                                             \
                {                                                                                                 \
                    const int baseNdx = iterNdx * callCount + callNdx;                                            \
                    const TYPE0 arg0  = args0[baseNdx % DE_LENGTH_OF_ARRAY(args0)];                               \
                    const TYPE1 arg1  = args1[baseNdx % DE_LENGTH_OF_ARRAY(args1)];                               \
                    const TYPE2 arg2  = args2[baseNdx % DE_LENGTH_OF_ARRAY(args2)];                               \
                    const TYPE3 arg3  = args3[baseNdx % DE_LENGTH_OF_ARRAY(args3)];                               \
                    gl.FUNCNAME(arg0, arg1, arg2, arg3);                                                          \
                }                                                                                                 \
            }                                                                                                     \
        };                                                                                                        \
        addChild(new StateChangeCallTest_##NAME(m_context, #NAME, DESCRIPTION));                                  \
    } while (0);

#define ADD_ARG_CASE6(NAME, DESCRIPTION, FUNCNAME, TYPE0, ARGS0, TYPE1, ARGS1, TYPE2, ARGS2, TYPE3, ARGS3, TYPE4, \
                      ARGS4, TYPE5, ARGS5)                                                                        \
    do                                                                                                            \
    {                                                                                                             \
        class StateChangeCallTest_##NAME : public gls::StateChangeCallPerformanceCase                             \
        {                                                                                                         \
        public:                                                                                                   \
            StateChangeCallTest_##NAME(Context &context, const char *name, const char *description)               \
                : gls::StateChangeCallPerformanceCase(context.getTestContext(), context.getRenderContext(), name, \
                                                      description)                                                \
            {                                                                                                     \
            }                                                                                                     \
            virtual void execCalls(const glw::Functions &gl, int iterNdx, int callCount)                          \
            {                                                                                                     \
                const TYPE0 args0[] = ARGS0;                                                                      \
                const TYPE1 args1[] = ARGS1;                                                                      \
                const TYPE2 args2[] = ARGS2;                                                                      \
                const TYPE3 args3[] = ARGS3;                                                                      \
                const TYPE4 args4[] = ARGS4;                                                                      \
                const TYPE5 args5[] = ARGS5;                                                                      \
                for (int callNdx = 0; callNdx < callCount; callNdx++)                                             \
                {                                                                                                 \
                    const int baseNdx = iterNdx * callCount + callNdx;                                            \
                    const TYPE0 arg0  = args0[baseNdx % DE_LENGTH_OF_ARRAY(args0)];                               \
                    const TYPE1 arg1  = args1[baseNdx % DE_LENGTH_OF_ARRAY(args1)];                               \
                    const TYPE2 arg2  = args2[baseNdx % DE_LENGTH_OF_ARRAY(args2)];                               \
                    const TYPE3 arg3  = args3[baseNdx % DE_LENGTH_OF_ARRAY(args3)];                               \
                    const TYPE4 arg4  = args4[baseNdx % DE_LENGTH_OF_ARRAY(args4)];                               \
                    const TYPE5 arg5  = args5[baseNdx % DE_LENGTH_OF_ARRAY(args5)];                               \
                    gl.FUNCNAME(arg0, arg1, arg2, arg3, arg4, arg5);                                              \
                }                                                                                                 \
            }                                                                                                     \
        };                                                                                                        \
        addChild(new StateChangeCallTest_##NAME(m_context, #NAME, DESCRIPTION));                                  \
    } while (0);

void StateChangeCallTests::init(void)
{
    ADD_ARG_CASE1(enable, "Test cost of glEnable() calls", enable, GLenum,
                  ARG_LIST({GL_CULL_FACE, GL_POLYGON_OFFSET_FILL, GL_SAMPLE_ALPHA_TO_COVERAGE, GL_SAMPLE_COVERAGE,
                            GL_SCISSOR_TEST, GL_STENCIL_TEST, GL_DEPTH_TEST, GL_BLEND, GL_DITHER}))

    ADD_ARG_CASE1(disable, "Test cost of glDisable() calls", disable, GLenum,
                  ARG_LIST({GL_CULL_FACE, GL_POLYGON_OFFSET_FILL, GL_SAMPLE_ALPHA_TO_COVERAGE, GL_SAMPLE_COVERAGE,
                            GL_SCISSOR_TEST, GL_STENCIL_TEST, GL_DEPTH_TEST, GL_BLEND, GL_DITHER}))

    ADD_ARG_CASE1(depth_func, "Test cost of glDepthFunc() calls", depthFunc, GLenum,
                  ARG_LIST({GL_NEVER, GL_ALWAYS, GL_LESS, GL_LEQUAL, GL_EQUAL, GL_GREATER, GL_GEQUAL, GL_NOTEQUAL}))

    ADD_ARG_CASE1(depth_mask, "Test cost of glDepthMask() calls", depthMask, GLboolean, ARG_LIST({GL_TRUE, GL_FALSE}))

    ADD_ARG_CASE1(stencil_mask, "Test cost of glStencilMask() calls", stencilMask, GLboolean,
                  ARG_LIST({GL_TRUE, GL_FALSE}))

    ADD_ARG_CASE1(clear_depth, "Test cost of glClearDepth() calls", clearDepthf, GLclampf, ARG_LIST({0.0f, 0.5f, 1.0f}))

    ADD_ARG_CASE1(clear_stencil, "Test cost of glClearStencil() calls", clearStencil, GLint, ARG_LIST({0, 128, 28}))

    ADD_ARG_CASE1(line_width, "Test cost of glLineWidth() calls", lineWidth, GLfloat, ARG_LIST({1.0f, 0.5f, 10.0f}))

    ADD_ARG_CASE1(cull_face, "Test cost of glCullFace() calls", cullFace, GLenum,
                  ARG_LIST({GL_FRONT, GL_BACK, GL_FRONT_AND_BACK}))

    ADD_ARG_CASE1(front_face, "Test cost of glFrontFace() calls", frontFace, GLenum, ARG_LIST({GL_CCW, GL_CW}))

    ADD_ARG_CASE1(blend_equation, "Test cost of glBlendEquation() calls", blendEquation, GLenum,
                  ARG_LIST({GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT}))

    ADD_ARG_CASE1(enable_vertex_attrib_array, "Test cost of glEnableVertexAttribArray() calls", enableVertexAttribArray,
                  GLuint,
                  ARG_LIST({
                      0,
                      1,
                      2,
                      3,
                      4,
                      5,
                      6,
                      7,
                  }))

    ADD_ARG_CASE1(disable_vertex_attrib_array, "Test cost of glDisableVertexAttribArray() calls",
                  disableVertexAttribArray, GLuint,
                  ARG_LIST({
                      0,
                      1,
                      2,
                      3,
                      4,
                      5,
                      6,
                      7,
                  }))

    ADD_ARG_CASE1(use_program, "Test cost of glUseProgram() calls. Note: Uses only program 0.", useProgram, GLuint,
                  ARG_LIST({
                      0,
                  }))

    ADD_ARG_CASE1(active_texture, "Test cost of glActiveTexture() calls", activeTexture, GLenum,
                  ARG_LIST({GL_TEXTURE0, GL_TEXTURE1, GL_TEXTURE2, GL_TEXTURE3, GL_TEXTURE4, GL_TEXTURE5, GL_TEXTURE6,
                            GL_TEXTURE7}))

    ADD_ARG_CASE2(depth_range, "Test cost of glDepthRangef() calls", depthRangef, GLclampf,
                  ARG_LIST({0.0f, 1.0f, 0.5f}), GLclampf, ARG_LIST({0.0f, 1.0f, 0.5f}))

    ADD_ARG_CASE2(polygon_offset, "Test cost of glPolygonOffset() calls", polygonOffset, GLfloat,
                  ARG_LIST({0.0f, 1.0f, 0.5f, 10.0f}), GLfloat, ARG_LIST({0.0f, 1.0f, 0.5f, 1000.0f}))

    ADD_ARG_CASE2(sample_coverage, "Test cost of glSampleCoverage() calls", sampleCoverage, GLclampf,
                  ARG_LIST({0.0f, 1.0f, 0.5f, 0.67f}), GLboolean, ARG_LIST({GL_TRUE, GL_FALSE}))

    ADD_ARG_CASE2(
        blend_func, "Test cost of glBlendFunc() calls", blendFunc, GLenum,
        ARG_LIST({GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR,
                  GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_CONSTANT_COLOR,
                  GL_ONE_MINUS_CONSTANT_COLOR, GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA}),
        GLenum,
        ARG_LIST({GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR,
                  GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_CONSTANT_COLOR,
                  GL_ONE_MINUS_CONSTANT_COLOR, GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA}))

    ADD_ARG_CASE2(blend_equation_separate, "Test cost of glBlendEquationSeparate() calls", blendEquationSeparate,
                  GLenum, ARG_LIST({GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT}), GLenum,
                  ARG_LIST({GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT}))

    ADD_ARG_CASE2(stencil_mask_separate, "Test cost of glStencilMaskSeparate() calls", stencilMaskSeparate, GLenum,
                  ARG_LIST({GL_FRONT, GL_BACK, GL_FRONT_AND_BACK}), GLboolean, ARG_LIST({GL_TRUE, GL_FALSE}))

    ADD_ARG_CASE2(bind_buffer, "Test cost of glBindBuffer() calls. Note: Uses only buffer 0", bindBuffer, GLenum,
                  ARG_LIST({GL_ELEMENT_ARRAY_BUFFER, GL_ARRAY_BUFFER}), GLuint, ARG_LIST({0}))

    ADD_ARG_CASE2(bind_texture, "Test cost of glBindTexture() calls. Note: Uses only texture 0", bindTexture, GLenum,
                  ARG_LIST({GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP}), GLuint, ARG_LIST({0}))

    ADD_ARG_CASE2(hint, "Test cost of glHint() calls", hint, GLenum, ARG_LIST({GL_GENERATE_MIPMAP_HINT}), GLenum,
                  ARG_LIST({GL_FASTEST, GL_NICEST, GL_DONT_CARE}))

    ADD_ARG_CASE3(stencil_func, "Test cost of glStencilFunc() calls", stencilFunc, GLenum,
                  ARG_LIST({GL_NEVER, GL_ALWAYS, GL_LESS, GL_LEQUAL, GL_EQUAL, GL_GEQUAL, GL_GREATER, GL_NOTEQUAL}),
                  GLint, ARG_LIST({0, 1, 255, 128, 7}), GLuint, ARG_LIST({0, 1, 255, 128, 7, 0xFFFFFFFF}))

    ADD_ARG_CASE3(
        stencil_op, "Test cost of glStencilOp() calls", stencilOp, GLenum,
        ARG_LIST({GL_KEEP, GL_ZERO, GL_REPLACE, GL_INCR, GL_DECR, GL_INVERT, GL_INCR_WRAP, GL_DECR_WRAP}), GLenum,
        ARG_LIST({GL_KEEP, GL_ZERO, GL_REPLACE, GL_INCR, GL_DECR, GL_INVERT, GL_INCR_WRAP, GL_DECR_WRAP}), GLenum,
        ARG_LIST({GL_KEEP, GL_ZERO, GL_REPLACE, GL_INCR, GL_DECR, GL_INVERT, GL_INCR_WRAP, GL_DECR_WRAP}))

    ADD_ARG_CASE4(viewport, "Test cost of glViewport() calls", viewport, GLint, ARG_LIST({0, 1, 100, 1145235}), GLint,
                  ARG_LIST({0, 1, 100, 1145235}), GLint, ARG_LIST({0, 1, 100, 1145235}), GLint,
                  ARG_LIST({0, 1, 100, 1145235}))

    ADD_ARG_CASE4(scissor, "Test cost of glScissor() calls", scissor, GLint, ARG_LIST({0, 1, 100, 1145235}), GLint,
                  ARG_LIST({0, 1, 100, 1145235}), GLint, ARG_LIST({0, 1, 100, 1145235}), GLint,
                  ARG_LIST({0, 1, 100, 1145235}))

    ADD_ARG_CASE4(stencil_func_separate, "Test cost of glStencilFuncSeparate() calls", stencilFuncSeparate, GLenum,
                  ARG_LIST({GL_FRONT, GL_BACK, GL_FRONT_AND_BACK}), GLenum,
                  ARG_LIST({GL_NEVER, GL_ALWAYS, GL_LESS, GL_LEQUAL, GL_EQUAL, GL_GEQUAL, GL_GREATER, GL_NOTEQUAL}),
                  GLint, ARG_LIST({0, 1, 255, 128, 7}), GLuint, ARG_LIST({0, 1, 255, 128, 7, 0xFFFFFFFF}))

    ADD_ARG_CASE4(
        stencil_op_separatae, "Test cost of glStencilOpSeparate() calls", stencilOpSeparate, GLenum,
        ARG_LIST({GL_FRONT, GL_BACK, GL_FRONT_AND_BACK}), GLenum,
        ARG_LIST({GL_KEEP, GL_ZERO, GL_REPLACE, GL_INCR, GL_DECR, GL_INVERT, GL_INCR_WRAP, GL_DECR_WRAP}), GLenum,
        ARG_LIST({GL_KEEP, GL_ZERO, GL_REPLACE, GL_INCR, GL_DECR, GL_INVERT, GL_INCR_WRAP, GL_DECR_WRAP}), GLenum,
        ARG_LIST({GL_KEEP, GL_ZERO, GL_REPLACE, GL_INCR, GL_DECR, GL_INVERT, GL_INCR_WRAP, GL_DECR_WRAP}))

    ADD_ARG_CASE4(
        blend_func_separate, "Test cost of glBlendFuncSeparate() calls", blendFuncSeparate, GLenum,
        ARG_LIST({GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR,
                  GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_CONSTANT_COLOR,
                  GL_ONE_MINUS_CONSTANT_COLOR, GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA}),
        GLenum,
        ARG_LIST({GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR,
                  GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_CONSTANT_COLOR,
                  GL_ONE_MINUS_CONSTANT_COLOR, GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA}),
        GLenum,
        ARG_LIST({GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR,
                  GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_CONSTANT_COLOR,
                  GL_ONE_MINUS_CONSTANT_COLOR, GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA}),
        GLenum,
        ARG_LIST({GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR,
                  GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_CONSTANT_COLOR,
                  GL_ONE_MINUS_CONSTANT_COLOR, GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA}))

    ADD_ARG_CASE4(color_mask, "Test cost of glColorMask() calls", colorMask, GLboolean, ARG_LIST({GL_TRUE, GL_FALSE}),
                  GLboolean, ARG_LIST({GL_TRUE, GL_FALSE}), GLboolean, ARG_LIST({GL_TRUE, GL_FALSE}), GLboolean,
                  ARG_LIST({GL_TRUE, GL_FALSE}))

    ADD_ARG_CASE4(clear_color, "Test cost of glClearColor() calls", clearColor, GLclampf,
                  ARG_LIST({0.0f, 1.0f, 0.5f, 0.33f}), GLclampf, ARG_LIST({0.0f, 1.0f, 0.5f, 0.33f}), GLclampf,
                  ARG_LIST({0.0f, 1.0f, 0.5f, 0.33f}), GLclampf, ARG_LIST({0.0f, 1.0f, 0.5f, 0.33f}))

    ADD_ARG_CASE6(vertex_attrib_pointer, "Test cost of glVertexAttribPointer() calls", vertexAttribPointer, GLuint,
                  ARG_LIST({0, 1, 2, 3, 4, 5, 6, 7}), GLint, ARG_LIST({1, 2, 3, 4}), GLenum,
                  ARG_LIST({GL_UNSIGNED_BYTE, GL_BYTE, GL_UNSIGNED_SHORT, GL_SHORT, GL_FLOAT}), GLboolean,
                  ARG_LIST({GL_FALSE, GL_TRUE}), GLsizei, ARG_LIST({0, 2, 4}), void *,
                  ARG_LIST({(void *)(uintptr_t)(0x0FF), (void *)(uintptr_t)(0x0EF)}))
}

} // namespace Performance
} // namespace gles2
} // namespace deqp
