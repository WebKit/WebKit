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
 * \brief Long-running stress tests.
 *//*--------------------------------------------------------------------*/

#include "es2sLongRunningTests.hpp"
#include "glsLongStressCase.hpp"
#include "glsLongStressTestUtil.hpp"
#include "glwEnums.hpp"

#include <string>

using std::string;

namespace deqp
{
namespace gles2
{
namespace Stress
{

LongRunningTests::LongRunningTests(Context &context) : TestCaseGroup(context, "long", "Long-running stress tests")
{
}

LongRunningTests::~LongRunningTests(void)
{
}

void LongRunningTests::init(void)
{
    static const int Mi = 1 << 20;
    const gls::LongStressTestUtil::ProgramLibrary progLib(glu::GLSL_VERSION_100_ES);

    typedef gls::LongStressCase::FeatureProbabilities Probs;

    // Buffer cases.

    {
        static const struct MemCase
        {
            const char *const nameSuffix;
            const char *const descSuffix;
            const int limit;
            const int redundantBufferFactor;
            MemCase(const char *n, const char *d, int l, int r)
                : nameSuffix(n)
                , descSuffix(d)
                , limit(l)
                , redundantBufferFactor(r)
            {
            }
        } memoryLimitCases[] = {MemCase("_low_memory", "; use a low buffer memory usage limit", 8 * Mi, 2),
                                MemCase("_high_memory", "; use a high buffer memory usage limit", 256 * Mi, 64)};

        const std::vector<gls::ProgramContext> contexts(1, progLib.generateBufferContext(4));

        static const struct Case
        {
            const char *const name;
            const char *const desc;
            const int redundantBufferFactor; //!< If non-positive, taken from memoryLimitCases.
            const Probs probs;
            Case(const char *const name_, const char *const desc_, int bufFact, const Probs &probs_ = Probs())
                : name(name_)
                , desc(desc_)
                , redundantBufferFactor(bufFact)
                , probs(probs_)
            {
            }
        } cases[] = {Case("always_reupload", "Re-upload buffer data at the beginning of each iteration", -1,
                          Probs().pReuploadBuffer(1.0f)),

                     Case("always_reupload_bufferdata",
                          "Re-upload buffer data at the beginning of each iteration, using glBufferData", -1,
                          Probs().pReuploadBuffer(1.0f).pReuploadWithBufferData(1.0f)),

                     Case("always_delete",
                          "Delete buffers at the end of each iteration, and re-create at the beginning of the next", -1,
                          Probs().pDeleteBuffer(1.0f)),

                     Case("wasteful", "Don't reuse buffers, and only delete them when given memory limit is reached", 2,
                          Probs().pWastefulBufferMemoryUsage(1.0f)),

                     Case("separate_attribute_buffers_wasteful", "Give each vertex attribute its own buffer", 2,
                          Probs().pSeparateAttribBuffers(1.0f).pWastefulBufferMemoryUsage(1.0f))};

        TestCaseGroup *const bufferGroup = new TestCaseGroup(m_context, "buffer", "Buffer stress tests");
        addChild(bufferGroup);

        for (int memoryLimitNdx = 0; memoryLimitNdx < DE_LENGTH_OF_ARRAY(memoryLimitCases); memoryLimitNdx++)
        {
            for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); caseNdx++)
            {
                const int redundantBufferFactor = cases[caseNdx].redundantBufferFactor > 0 ?
                                                      cases[caseNdx].redundantBufferFactor :
                                                      memoryLimitCases[memoryLimitNdx].redundantBufferFactor;

                bufferGroup->addChild(new gls::LongStressCase(
                    m_context.getTestContext(), m_context.getRenderContext(),
                    (string() + cases[caseNdx].name + memoryLimitCases[memoryLimitNdx].nameSuffix).c_str(),
                    (string() + cases[caseNdx].desc + memoryLimitCases[memoryLimitNdx].descSuffix).c_str(),
                    0 /* tex memory */, memoryLimitCases[memoryLimitNdx].limit, 1 /* draw calls per iteration */,
                    50000 /* tris per call */, contexts, cases[caseNdx].probs, GL_DYNAMIC_DRAW, GL_DYNAMIC_DRAW,
                    redundantBufferFactor));
            }
        }
    }

    // Texture cases.

    {
        static const struct MemCase
        {
            const char *const nameSuffix;
            const char *const descSuffix;
            const int limit;
            const int numTextures;
            MemCase(const char *n, const char *d, int l, int t) : nameSuffix(n), descSuffix(d), limit(l), numTextures(t)
            {
            }
        } memoryLimitCases[] = {MemCase("_low_memory", "; use a low texture memory usage limit", 8 * Mi, 6),
                                MemCase("_high_memory", "; use a high texture memory usage limit", 256 * Mi, 192)};

        static const struct Case
        {
            const char *const name;
            const char *const desc;
            const int numTextures; //!< If non-positive, taken from memoryLimitCases.
            const Probs probs;
            Case(const char *const name_, const char *const desc_, int numTextures_, const Probs &probs_ = Probs())
                : name(name_)
                , desc(desc_)
                , numTextures(numTextures_)
                , probs(probs_)
            {
            }
        } cases[] = {Case("always_reupload", "Re-upload texture data at the beginning of each iteration", -1,
                          Probs().pReuploadTexture(1.0f)),

                     Case("always_reupload_teximage",
                          "Re-upload texture data at the beginning of each iteration, using glTexImage*", -1,
                          Probs().pReuploadTexture(1.0f).pReuploadWithTexImage(1.0f)),

                     Case("always_delete",
                          "Delete textures at the end of each iteration, and re-create at the beginning of the next",
                          -1, Probs().pDeleteTexture(1.0f)),

                     Case("wasteful", "Don't reuse textures, and only delete them when given memory limit is reached",
                          6, Probs().pWastefulTextureMemoryUsage(1.0f))};

        TestCaseGroup *const textureGroup = new TestCaseGroup(m_context, "texture", "Texture stress tests");
        addChild(textureGroup);

        for (int memoryLimitNdx = 0; memoryLimitNdx < DE_LENGTH_OF_ARRAY(memoryLimitCases); memoryLimitNdx++)
        {
            for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); caseNdx++)
            {
                const int numTextures = cases[caseNdx].numTextures > 0 ? cases[caseNdx].numTextures :
                                                                         memoryLimitCases[memoryLimitNdx].numTextures;
                const std::vector<gls::ProgramContext> contexts(
                    1, progLib.generateTextureContext(numTextures, 512, 512, 0.1f));

                textureGroup->addChild(new gls::LongStressCase(
                    m_context.getTestContext(), m_context.getRenderContext(),
                    (string() + cases[caseNdx].name + memoryLimitCases[memoryLimitNdx].nameSuffix).c_str(),
                    (string() + cases[caseNdx].desc + memoryLimitCases[memoryLimitNdx].descSuffix).c_str(),
                    memoryLimitCases[memoryLimitNdx].limit, 1 * Mi /* buf memory */, 1 /* draw calls per iteration */,
                    10000 /* tris per call */, contexts, cases[caseNdx].probs, GL_STATIC_DRAW, GL_STATIC_DRAW));
            }
        }
    }

    // Draw call cases.

    {
        const std::vector<gls::ProgramContext> contexts(1, progLib.generateTextureContext(1, 128, 128, 0.5f));

        static const struct Case
        {
            const char *const name;
            const char *const desc;
            const int drawCallsPerIteration;
            const int numTrisPerDrawCall;
            const Probs probs;
            Case(const char *const name_, const char *const desc_, const int calls, const int tris,
                 const Probs &probs_ = Probs())
                : name(name_)
                , desc(desc_)
                , drawCallsPerIteration(calls)
                , numTrisPerDrawCall(tris)
                , probs(probs_)
            {
            }
        } cases[] = {Case("client_memory_data", "Use client-memory for index and attribute data, instead of GL buffers",
                          200, 500, Probs().pClientMemoryAttributeData(1.0f).pClientMemoryIndexData(1.0f)),

                     Case("vary_draw_function",
                          "Choose between glDrawElements and glDrawArrays each iteration, with uniform probability",
                          200, 500, Probs().pUseDrawArrays(0.5f)),

                     Case("few_big_calls", "Per iteration, do a few draw calls with a big number of triangles per call",
                          2, 50000),

                     Case("many_small_calls",
                          "Per iteration, do many draw calls with a small number of triangles per call", 2000, 50)};

        TestCaseGroup *const drawCallGroup = new TestCaseGroup(m_context, "draw_call", "Draw call stress tests");
        addChild(drawCallGroup);

        for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); caseNdx++)
        {
            drawCallGroup->addChild(new gls::LongStressCase(
                m_context.getTestContext(), m_context.getRenderContext(), cases[caseNdx].name, cases[caseNdx].desc,
                1 * Mi /* tex memory */, 2 * Mi /* buf memory */, cases[caseNdx].drawCallsPerIteration,
                cases[caseNdx].numTrisPerDrawCall, contexts, cases[caseNdx].probs, GL_STATIC_DRAW, GL_STATIC_DRAW));
        }
    }

    // Shader cases.

    {
        std::vector<gls::ProgramContext> contexts;
        contexts.push_back(progLib.generateFragmentPointLightContext(512, 512));
        contexts.push_back(progLib.generateVertexUniformLoopLightContext(512, 512));

        static const struct Case
        {
            const char *const name;
            const char *const desc;
            const Probs probs;
            Case(const char *const name_, const char *const desc_, const Probs &probs_ = Probs())
                : name(name_)
                , desc(desc_)
                , probs(probs_)
            {
            }
        } cases[] = {Case("several_programs",
                          "Use several different programs, choosing between them uniformly on each iteration"),

                     Case("several_programs_always_rebuild",
                          "Use several different programs, choosing between them uniformly on each iteration, and "
                          "always rebuild the program",
                          Probs().pRebuildProgram(1.0f))};

        TestCaseGroup *const shaderGroup = new TestCaseGroup(m_context, "program", "Shader program stress tests");
        addChild(shaderGroup);

        for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(cases); caseNdx++)
        {
            shaderGroup->addChild(new gls::LongStressCase(
                m_context.getTestContext(), m_context.getRenderContext(), cases[caseNdx].name, cases[caseNdx].desc,
                3 * Mi /* tex memory */, 1 * Mi /* buf memory */, 1 /* draw calls per iteration */,
                10000 /* tris per call */, contexts, cases[caseNdx].probs, GL_STATIC_DRAW, GL_STATIC_DRAW));
        }
    }

    // Mixed cases.

    {
        static const struct MemCase
        {
            const char *const nameSuffix;
            const char *const descSuffix;
            const int texLimit;
            const int bufLimit;
            MemCase(const char *n, const char *d, int t, int b) : nameSuffix(n), descSuffix(d), texLimit(t), bufLimit(b)
            {
            }
        } memoryLimitCases[] = {MemCase("_low_memory", "; use a low memory usage limit", 8 * Mi, 8 * Mi),
                                MemCase("_high_memory", "; use a high memory usage limit", 128 * Mi, 128 * Mi)};

        TestCaseGroup *const mixedGroup = new TestCaseGroup(m_context, "mixed", "Mixed stress tests");
        addChild(mixedGroup);

        for (int memoryLimitNdx = 0; memoryLimitNdx < DE_LENGTH_OF_ARRAY(memoryLimitCases); memoryLimitNdx++)
        {
            mixedGroup->addChild(new gls::LongStressCase(
                m_context.getTestContext(), m_context.getRenderContext(),
                (string() + "buffer_texture_wasteful" + memoryLimitCases[memoryLimitNdx].nameSuffix).c_str(),
                (string() + "Use both buffers and textures wastefully" + memoryLimitCases[memoryLimitNdx].descSuffix)
                    .c_str(),
                memoryLimitCases[memoryLimitNdx].texLimit, memoryLimitCases[memoryLimitNdx].bufLimit,
                1 /* draw calls per iteration */, 10000 /* tris per call */,
                std::vector<gls::ProgramContext>(1, progLib.generateBufferAndTextureContext(4, 512, 512)),
                Probs()
                    .pReuploadTexture(0.3f)
                    .pReuploadWithTexImage(0.5f)
                    .pReuploadBuffer(0.3f)
                    .pReuploadWithBufferData(0.5f)
                    .pDeleteTexture(0.2f)
                    .pDeleteBuffer(0.2f)
                    .pWastefulTextureMemoryUsage(0.5f)
                    .pWastefulBufferMemoryUsage(0.5f)
                    .pRandomBufferUploadTarget(1.0f)
                    .pRandomBufferUsage(1.0f),
                GL_STATIC_DRAW, GL_STATIC_DRAW));

            {
                std::vector<gls::ProgramContext> contexts;
                contexts.push_back(progLib.generateFragmentPointLightContext(512, 512));
                contexts.push_back(progLib.generateVertexUniformLoopLightContext(512, 512));
                mixedGroup->addChild(new gls::LongStressCase(
                    m_context.getTestContext(), m_context.getRenderContext(),
                    (string() + "random" + memoryLimitCases[memoryLimitNdx].nameSuffix).c_str(),
                    (string() + "Highly random behavior" + memoryLimitCases[memoryLimitNdx].descSuffix).c_str(),
                    memoryLimitCases[memoryLimitNdx].texLimit, memoryLimitCases[memoryLimitNdx].bufLimit,
                    1 /* draw calls per iteration */, 10000 /* tris per call */, contexts,
                    Probs()
                        .pRebuildProgram(0.3f)
                        .pReuploadTexture(0.3f)
                        .pReuploadWithTexImage(0.3f)
                        .pReuploadBuffer(0.3f)
                        .pReuploadWithBufferData(0.3f)
                        .pDeleteTexture(0.2f)
                        .pDeleteBuffer(0.2f)
                        .pWastefulTextureMemoryUsage(0.3f)
                        .pWastefulBufferMemoryUsage(0.3f)
                        .pClientMemoryAttributeData(0.2f)
                        .pClientMemoryIndexData(0.2f)
                        .pSeparateAttribBuffers(0.4f)
                        .pUseDrawArrays(0.4f)
                        .pRandomBufferUploadTarget(1.0f)
                        .pRandomBufferUsage(1.0f),
                    GL_STATIC_DRAW, GL_STATIC_DRAW));
            }
        }
    }
}

} // namespace Stress
} // namespace gles2
} // namespace deqp
