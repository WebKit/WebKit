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
 * \brief Sync tests.
 *//*--------------------------------------------------------------------*/

#include "es3fSyncTests.hpp"

#include "tcuTestLog.hpp"
#include "tcuVector.hpp"
#include "gluShaderProgram.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluRenderContext.hpp"
#include "glwEnums.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deString.h"

#include <vector>

using tcu::TestLog;

namespace deqp
{
namespace gles3
{
namespace Functional
{

using namespace glw; // GL types

static const int NUM_CASE_ITERATIONS = 5;

enum WaitCommand
{
    COMMAND_WAIT_SYNC        = 1 << 0,
    COMMAND_CLIENT_WAIT_SYNC = 1 << 1
};

enum CaseOptions
{
    CASE_FLUSH_BEFORE_WAIT  = 1 << 0,
    CASE_FINISH_BEFORE_WAIT = 1 << 1
};

class FenceSyncCase : public TestCase, private glu::CallLogWrapper
{
public:
    FenceSyncCase(Context &context, const char *name, const char *description, int numPrimitives, uint32_t waitCommand,
                  uint32_t waitFlags, uint64_t timeout, uint32_t options);
    ~FenceSyncCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    FenceSyncCase(const FenceSyncCase &other);
    FenceSyncCase &operator=(const FenceSyncCase &other);

    int m_numPrimitives;
    uint32_t m_waitCommand;
    uint32_t m_waitFlags;
    uint64_t m_timeout;
    uint32_t m_caseOptions;

    glu::ShaderProgram *m_program;
    GLsync m_syncObject;
    int m_iterNdx;
    de::Random m_rnd;
};

FenceSyncCase::FenceSyncCase(Context &context, const char *name, const char *description, int numPrimitives,
                             uint32_t waitCommand, uint32_t waitFlags, uint64_t timeout, uint32_t options)
    : TestCase(context, name, description)
    , CallLogWrapper(context.getRenderContext().getFunctions(), context.getTestContext().getLog())
    , m_numPrimitives(numPrimitives)
    , m_waitCommand(waitCommand)
    , m_waitFlags(waitFlags)
    , m_timeout(timeout)
    , m_caseOptions(options)
    , m_program(nullptr)
    , m_syncObject(nullptr)
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

    if (m_syncObject)
    {
        glDeleteSync(m_syncObject);
        m_syncObject = nullptr;
    }
}

FenceSyncCase::IterateResult FenceSyncCase::iterate(void)
{
    TestLog &log = m_testCtx.getLog();
    std::vector<float> vertices;
    bool testOk = true;

    std::string header = "Case iteration " + de::toString(m_iterNdx + 1) + " / " + de::toString(NUM_CASE_ITERATIONS);
    const tcu::ScopedLogSection section(log, header, header);

    enableLogging(true);

    DE_ASSERT(m_program);
    glUseProgram(m_program->getProgram());
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Generate vertices

    glEnableVertexAttribArray(0);
    generateVertices(vertices, m_numPrimitives, m_rnd);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, &vertices[0]);

    // Draw

    glDrawArrays(GL_TRIANGLES, 0, (int)vertices.size() / 4);
    log << TestLog::Message << "// Primitives drawn." << TestLog::EndMessage;

    // Create sync object

    m_syncObject = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    GLU_CHECK_MSG("Sync object created");
    log << TestLog::Message << "// Sync object created." << TestLog::EndMessage;

    if (m_caseOptions & CASE_FLUSH_BEFORE_WAIT)
        glFlush();
    if (m_caseOptions & CASE_FINISH_BEFORE_WAIT)
        glFinish();

    // Wait for sync object

    GLenum waitValue = 0;

    if (m_waitCommand & COMMAND_WAIT_SYNC)
    {
        DE_ASSERT(m_timeout == GL_TIMEOUT_IGNORED);
        DE_ASSERT(m_waitFlags == 0);
        glWaitSync(m_syncObject, m_waitFlags, m_timeout);
        GLU_CHECK_MSG("glWaitSync called");
        log << TestLog::Message << "// Wait command glWaitSync called with GL_TIMEOUT_IGNORED." << TestLog::EndMessage;
    }
    if (m_waitCommand & COMMAND_CLIENT_WAIT_SYNC)
    {
        waitValue = glClientWaitSync(m_syncObject, m_waitFlags, m_timeout);
        GLU_CHECK_MSG("glClientWaitSync called");
        log << TestLog::Message << "// glClientWaitSync return value:" << TestLog::EndMessage;
        switch (waitValue)
        {
        case GL_ALREADY_SIGNALED:
            log << TestLog::Message << "// GL_ALREADY_SIGNALED" << TestLog::EndMessage;
            break;
        case GL_TIMEOUT_EXPIRED:
            log << TestLog::Message << "// GL_TIMEOUT_EXPIRED" << TestLog::EndMessage;
            break;
        case GL_CONDITION_SATISFIED:
            log << TestLog::Message << "// GL_CONDITION_SATISFIED" << TestLog::EndMessage;
            break;
        case GL_WAIT_FAILED:
            log << TestLog::Message << "// GL_WAIT_FAILED" << TestLog::EndMessage;
            testOk = false;
            break;
        default:
            TCU_FAIL("// Illegal return value!");
        }
    }

    glFinish();

    if (m_caseOptions & CASE_FINISH_BEFORE_WAIT && waitValue != GL_ALREADY_SIGNALED)
    {
        testOk = false;
        log << TestLog::Message << "// Expected glClientWaitSync to return GL_ALREADY_SIGNALED." << TestLog::EndMessage;
    }

    // Delete sync object

    if (m_syncObject)
    {
        glDeleteSync(m_syncObject);
        m_syncObject = nullptr;
        GLU_CHECK_MSG("Sync object deleted");
        log << TestLog::Message << "// Sync object deleted." << TestLog::EndMessage;
    }

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
    // Fence sync tests.

    addChild(new FenceSyncCase(m_context, "wait_sync_smalldraw", "", 10, COMMAND_WAIT_SYNC, 0, GL_TIMEOUT_IGNORED, 0));
    addChild(
        new FenceSyncCase(m_context, "wait_sync_largedraw", "", 10000, COMMAND_WAIT_SYNC, 0, GL_TIMEOUT_IGNORED, 0));

    addChild(new FenceSyncCase(m_context, "client_wait_sync_smalldraw", "", 10, COMMAND_CLIENT_WAIT_SYNC, 0, 0, 0));
    addChild(new FenceSyncCase(m_context, "client_wait_sync_largedraw", "", 10000, COMMAND_CLIENT_WAIT_SYNC, 0, 0, 0));
    addChild(
        new FenceSyncCase(m_context, "client_wait_sync_timeout_smalldraw", "", 10, COMMAND_CLIENT_WAIT_SYNC, 0, 10, 0));
    addChild(new FenceSyncCase(m_context, "client_wait_sync_timeout_largedraw", "", 10000, COMMAND_CLIENT_WAIT_SYNC, 0,
                               10, 0));

    addChild(new FenceSyncCase(m_context, "client_wait_sync_flush_auto", "", 10000, COMMAND_CLIENT_WAIT_SYNC,
                               GL_SYNC_FLUSH_COMMANDS_BIT, 0, 0));
    addChild(new FenceSyncCase(m_context, "client_wait_sync_flush_manual", "", 10000, COMMAND_CLIENT_WAIT_SYNC, 0, 0,
                               CASE_FLUSH_BEFORE_WAIT));
    addChild(new FenceSyncCase(m_context, "client_wait_sync_noflush", "", 10000, COMMAND_CLIENT_WAIT_SYNC, 0, 0, 0));
    addChild(new FenceSyncCase(m_context, "client_wait_sync_finish", "", 10000, COMMAND_CLIENT_WAIT_SYNC, 0, 0,
                               CASE_FINISH_BEFORE_WAIT));
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
