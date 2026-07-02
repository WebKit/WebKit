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
 * \brief Sync stress tests.
 *//*--------------------------------------------------------------------*/

#include "es3sSyncTests.hpp"

#include "tcuTestLog.hpp"
#include "deRandom.hpp"
#include "tcuVector.hpp"
#include "gluShaderProgram.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluRenderContext.hpp"
#include "deStringUtil.hpp"
#include "deString.h"

#include <vector>

#include "glw.h"

using std::vector;
using tcu::TestLog;

namespace deqp
{
namespace gles3
{
namespace Stress
{

static const int NUM_CASE_ITERATIONS = 1;

enum WaitCommand
{
    COMMAND_WAIT_SYNC        = 1 << 0,
    COMMAND_CLIENT_WAIT_SYNC = 1 << 1
};

class FenceSyncCase : public TestCase, public glu::CallLogWrapper
{
public:
    FenceSyncCase(Context &context, const char *name, const char *description, int numPrimitives, uint32_t waitCommand);
    ~FenceSyncCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    FenceSyncCase(const FenceSyncCase &other);
    FenceSyncCase &operator=(const FenceSyncCase &other);

    int m_numSyncs;
    uint32_t m_waitCommand;

    glu::ShaderProgram *m_program;
    vector<GLsync> m_syncObjects;
    int m_iterNdx;
    de::Random m_rnd;
};

FenceSyncCase::FenceSyncCase(Context &context, const char *name, const char *description, int numSyncs,
                             uint32_t waitCommand)
    : TestCase(context, name, description)
    , CallLogWrapper(context.getRenderContext().getFunctions(), context.getTestContext().getLog())
    , m_numSyncs(numSyncs)
    , m_waitCommand(waitCommand)
    , m_program(nullptr)
    , m_iterNdx(0)
    , m_rnd(deStringHash(name))
{
}

FenceSyncCase::~FenceSyncCase(void)
{
    FenceSyncCase::deinit();
}

static void generateVertices(std::vector<float> &dst, int numPrimitives, de::Random &rnd)
{
    int numVertices = 3 * numPrimitives;
    dst.resize(numVertices * 4);

    for (int i = 0; i < numVertices; i++)
    {
        dst[i * 4]     = rnd.getFloat(-1.0f, 1.0f); // x
        dst[i * 4 + 1] = rnd.getFloat(-1.0f, 1.0f); // y
        dst[i * 4 + 2] = rnd.getFloat(0.0f, 1.0f);  // z
        dst[i * 4 + 3] = 1.0f;                      // w
    }
}

void FenceSyncCase::init(void)
{
    const char *vertShaderSource = "#version 300 es\n"
                                   "layout(location = 0) in mediump vec4 a_position;\n"
                                   "\n"
                                   "void main (void)\n"
                                   "{\n"
                                   "    gl_Position = a_position;\n"
                                   "}\n";

    const char *fragShaderSource = "#version 300 es\n"
                                   "layout(location = 0) out mediump vec4 o_color;\n"
                                   "\n"
                                   "void main (void)\n"
                                   "{\n"
                                   "    o_color = vec4(0.25, 0.5, 0.75, 1.0);\n"
                                   "}\n";

    DE_ASSERT(!m_program);
    m_program = new glu::ShaderProgram(m_context.getRenderContext(),
                                       glu::makeVtxFragSources(vertShaderSource, fragShaderSource));

    if (!m_program->isOk())
    {
        m_testCtx.getLog() << *m_program;
        TCU_FAIL("Failed to compile shader program");
    }

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass"); // Initialize test result to pass.
    GLU_CHECK_MSG("Case initialization finished");
}

void FenceSyncCase::deinit(void)
{
    if (m_program)
    {
        delete m_program;
        m_program = nullptr;
    }

    for (int i = 0; i < (int)m_syncObjects.size(); i++)
        if (m_syncObjects[i])
            glDeleteSync(m_syncObjects[i]);

    m_syncObjects.erase(m_syncObjects.begin(), m_syncObjects.end());
}

FenceSyncCase::IterateResult FenceSyncCase::iterate(void)
{
    TestLog &log = m_testCtx.getLog();
    std::vector<float> vertices;
    bool testOk = true;

    std::string header = "Case iteration " + de::toString(m_iterNdx + 1) + " / " + de::toString(NUM_CASE_ITERATIONS);
    tcu::ScopedLogSection section(log, header, header);

    enableLogging(true);

    TCU_CHECK(m_program);
    glUseProgram(m_program->getProgram());
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Generate vertices

    glEnableVertexAttribArray(0);
    generateVertices(vertices, m_numSyncs, m_rnd);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, &vertices[0]);
    m_syncObjects.resize(m_numSyncs);

    // Perform draws and create sync objects

    enableLogging(false);
    log << TestLog::Message << "// NOT LOGGED: " << m_numSyncs << " glDrawArrays and glFenceSync calls done here."
        << TestLog::EndMessage;

    for (int i = 0; i < m_numSyncs; i++)
    {
        glDrawArrays(GL_TRIANGLES, i * 3, 3);
        m_syncObjects[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        GLU_CHECK_MSG("Sync object created");
    }
    enableLogging(true);
    log << TestLog::Message << "// Draws performed, sync objects created." << TestLog::EndMessage;

    // Wait for sync objects

    m_rnd.shuffle(m_syncObjects.begin(), m_syncObjects.end());

    enableLogging(false);
    if (m_waitCommand & COMMAND_WAIT_SYNC)
        log << TestLog::Message << "// NOT LOGGED: " << m_numSyncs << " glWaitSync calls done here."
            << TestLog::EndMessage;
    else if (m_waitCommand & COMMAND_CLIENT_WAIT_SYNC)
        log << TestLog::Message << "// NOT LOGGED: " << m_numSyncs << " glClientWaitSync calls done here."
            << TestLog::EndMessage;

    for (int i = 0; i < m_numSyncs; i++)
    {
        GLenum waitValue = 0;

        if (m_waitCommand & COMMAND_WAIT_SYNC)
        {
            glWaitSync(m_syncObjects[i], 0, GL_TIMEOUT_IGNORED);
            GLU_CHECK_MSG("glWaitSync called");
        }
        else if (m_waitCommand & COMMAND_CLIENT_WAIT_SYNC)
        {
            waitValue = glClientWaitSync(m_syncObjects[i], 0, 100);
            GLU_CHECK_MSG("glClientWaitSync called");
            switch (waitValue)
            {
            case GL_ALREADY_SIGNALED:
                break;
            case GL_TIMEOUT_EXPIRED:
                break;
            case GL_CONDITION_SATISFIED:
                break;
            case GL_WAIT_FAILED:
                log << TestLog::Message << "// glClientWaitSync returned GL_WAIT_FAILED" << TestLog::EndMessage;
                testOk = false;
                break;
            default:
                TCU_FAIL("glClientWaitSync returned an unknown return value.");
            }
        }
    }
    enableLogging(true);

    glFinish();

    // Delete sync objects

    enableLogging(false);
    log << TestLog::Message << "// NOT LOGGED: " << m_numSyncs << " glDeleteSync calls done here."
        << TestLog::EndMessage;

    for (int i = 0; i < (int)m_syncObjects.size(); i++)
    {
        if (m_syncObjects[i])
        {
            glDeleteSync(m_syncObjects[i]);
            GLU_CHECK_MSG("Sync object deleted");
        }
    }

    enableLogging(true);

    m_syncObjects.erase(m_syncObjects.begin(), m_syncObjects.end());

    // Evaluate test result

    log << TestLog::Message << "// Test result: " << (testOk ? "Passed!" : "Failed!") << TestLog::EndMessage;

    if (!testOk)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return STOP;
    }

    log << TestLog::Message << "// Sync objects created and deleted successfully." << TestLog::EndMessage;

    return (++m_iterNdx < NUM_CASE_ITERATIONS) ? CONTINUE : STOP;
}

SyncTests::SyncTests(Context &context) : TestCaseGroup(context, "fence_sync", "Fence Sync Tests")
{
}

SyncTests::~SyncTests(void)
{
}

void SyncTests::init(void)
{
    // Fence sync stress tests.

    addChild(new FenceSyncCase(m_context, "wait_sync_10_syncs", "", 10, COMMAND_WAIT_SYNC));
    addChild(new FenceSyncCase(m_context, "wait_sync_1000_syncs", "", 1000, COMMAND_WAIT_SYNC));
    addChild(new FenceSyncCase(m_context, "wait_sync_10000_syncs", "", 10000, COMMAND_WAIT_SYNC));

    addChild(new FenceSyncCase(m_context, "client_wait_sync_10_syncs", "", 10, COMMAND_CLIENT_WAIT_SYNC));
    addChild(new FenceSyncCase(m_context, "client_wait_sync_1000_syncs", "", 1000, COMMAND_CLIENT_WAIT_SYNC));
    addChild(new FenceSyncCase(m_context, "client_wait_sync_10000_syncs", "", 10000, COMMAND_CLIENT_WAIT_SYNC));
}

} // namespace Stress
} // namespace gles3
} // namespace deqp
