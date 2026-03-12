//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DebugTest.cpp : Tests of the GL_KHR_debug extension

#include "common/debug.h"
#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

namespace angle
{
constexpr char kBufferObjLabel[]          = "buffer";
constexpr char kShaderObjLabel[]          = "shader";
constexpr char kProgramObjLabel[]         = "program";
constexpr char kVertexArrayObjLabel[]     = "vertexarray";
constexpr char kQueryObjLabel[]           = "query";
constexpr char kProgramPipelineObjLabel[] = "programpipeline";
constexpr GLenum kObjectTypes[]           = {GL_BUFFER_OBJECT_EXT,           GL_SHADER_OBJECT_EXT,
                                             GL_PROGRAM_OBJECT_EXT,          GL_QUERY_OBJECT_EXT,
                                             GL_PROGRAM_PIPELINE_OBJECT_EXT, GL_VERTEX_ARRAY_OBJECT_EXT};

class DebugTest : public ANGLETest<>
{
  protected:
    DebugTest() : mDebugExtensionAvailable(false)
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
        setDebugEnabled(true);
        setExtensionsEnabled(false);
    }

    void testSetUp() override
    {
        mDebugExtensionAvailable = EnsureGLExtensionEnabled("GL_KHR_debug");
        if (mDebugExtensionAvailable)
        {
            glEnable(GL_DEBUG_OUTPUT);
        }
    }

    bool mDebugExtensionAvailable;
};

void createGLObjectAndLabel(GLenum identifier,
                            GLuint &object,
                            const char **label,
                            int major,
                            int minor)
{
    switch (identifier)
    {
        case GL_BUFFER_OBJECT_EXT:
            glGenBuffers(1, &object);
            glBindBuffer(GL_ARRAY_BUFFER, object);
            *label = kBufferObjLabel;
            break;
        case GL_SHADER_OBJECT_EXT:
            object = glCreateShader(GL_VERTEX_SHADER);
            *label = kShaderObjLabel;
            break;
        case GL_PROGRAM_OBJECT_EXT:
            object = glCreateProgram();
            *label = kProgramObjLabel;
            break;
        case GL_VERTEX_ARRAY_OBJECT_EXT:
            if (major < 3)
            {
                glGenVertexArraysOES(1, &object);
                glBindVertexArrayOES(object);
            }
            else
            {
                glGenVertexArrays(1, &object);
                glBindVertexArray(object);
            }
            *label = kVertexArrayObjLabel;
            break;
        case GL_QUERY_OBJECT_EXT:
            if (major < 3)
            {
                glGenQueriesEXT(1, &object);
                glBeginQueryEXT(GL_ANY_SAMPLES_PASSED, object);
            }
            else
            {
                glGenQueries(1, &object);
                glBeginQuery(GL_ANY_SAMPLES_PASSED, object);
            }
            *label = kQueryObjLabel;
            break;
        case GL_PROGRAM_PIPELINE_OBJECT_EXT:
            if (major < 3 || minor < 1)
            {
                glGenProgramPipelinesEXT(1, &object);
                glBindProgramPipelineEXT(object);
            }
            else
            {
                glGenProgramPipelines(1, &object);
                glBindProgramPipeline(object);
            }
            *label = kProgramPipelineObjLabel;
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void deleteGLObject(GLenum identifier, GLuint &object, int major, int minor)
{
    switch (identifier)
    {
        case GL_BUFFER_OBJECT_EXT:
            glDeleteBuffers(1, &object);
            break;
        case GL_SHADER_OBJECT_EXT:
            glDeleteShader(object);
            break;
        case GL_PROGRAM_OBJECT_EXT:
            glDeleteProgram(object);
            break;
        case GL_VERTEX_ARRAY_OBJECT_EXT:
            if (major < 3)
            {
                glDeleteVertexArraysOES(1, &object);
            }
            else
            {
                glDeleteVertexArrays(1, &object);
            }
            break;
        case GL_QUERY_OBJECT_EXT:
            if (major < 3)
            {
                glEndQueryEXT(GL_ANY_SAMPLES_PASSED);
                glDeleteQueriesEXT(1, &object);
            }
            else
            {
                glEndQuery(GL_ANY_SAMPLES_PASSED);
                glDeleteQueries(1, &object);
            }
            break;
        case GL_PROGRAM_PIPELINE_OBJECT_EXT:
            if (major < 3 || minor < 1)
            {
                glDeleteProgramPipelinesEXT(1, &object);
            }
            else
            {
                glDeleteProgramPipelines(1, &object);
            }
            break;
        default:
            UNREACHABLE();
            break;
    }
}

// Test basic usage of setting and getting labels using GL_EXT_debug_label
TEST_P(DebugTest, ObjectLabelsEXT)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_debug_label"));

    for (const GLenum identifier : kObjectTypes)
    {
        bool skip = false;
        switch (identifier)
        {
            case GL_PROGRAM_OBJECT_EXT:
            case GL_SHADER_OBJECT_EXT:
                if (getClientMajorVersion() < 2)
                {
                    skip = true;
                }
                break;
            case GL_PROGRAM_PIPELINE_OBJECT_EXT:
                if ((getClientMajorVersion() < 3 || getClientMinorVersion() < 1) &&
                    !EnsureGLExtensionEnabled("GL_EXT_separate_shader_objects"))
                {
                    skip = true;
                }
                break;
            case GL_QUERY_OBJECT_EXT:
                if (getClientMajorVersion() < 3 &&
                    !EnsureGLExtensionEnabled("GL_EXT_occlusion_query_boolean"))
                {
                    skip = true;
                }
                break;
            case GL_VERTEX_ARRAY_OBJECT_EXT:
                if (getClientMajorVersion() < 3 &&
                    !EnsureGLExtensionEnabled("GL_OES_vertex_array_object"))
                {
                    skip = true;
                }
                break;
            default:
                break;
        }

        // if object enum is not supported, move on to the next object type
        if (skip)
        {
            continue;
        }

        GLuint object;
        const char *label;
        createGLObjectAndLabel(identifier, object, &label, getClientMajorVersion(),
                               getClientMinorVersion());
        ASSERT_GL_NO_ERROR();

        glLabelObjectEXT(identifier, object, 0, label);
        ASSERT_GL_NO_ERROR();

        std::vector<char> labelBuf(strlen(label) + 1);
        GLsizei labelLengthBuf = 0;
        glGetObjectLabelEXT(identifier, object, static_cast<GLsizei>(labelBuf.size()),
                            &labelLengthBuf, labelBuf.data());
        ASSERT_GL_NO_ERROR();

        EXPECT_EQ(static_cast<GLsizei>(strlen(label)), labelLengthBuf);
        EXPECT_STREQ(label, labelBuf.data());

        deleteGLObject(identifier, object, getClientMajorVersion(), getClientMinorVersion());
        ASSERT_GL_NO_ERROR();

        glLabelObjectEXT(identifier, object, 0, label);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);

        glGetObjectLabelEXT(identifier, object, static_cast<GLsizei>(labelBuf.size()),
                            &labelLengthBuf, labelBuf.data());
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }
}

// Test basic usage of setting and getting labels using GL_EXT_debug_label on timer query objects
TEST_P(DebugTest, TimerQueryObjectLabelsEXT)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_debug_label"));

    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_disjoint_timer_query"));

    GLuint object;
    glGenQueriesEXT(1, &object);
    glBeginQueryEXT(GL_TIME_ELAPSED_EXT, object);
    ASSERT_GL_NO_ERROR();

    glLabelObjectEXT(GL_QUERY_OBJECT_EXT, object, 0, kQueryObjLabel);
    EXPECT_GL_NO_ERROR();

    std::vector<char> labelBuf(strlen(kQueryObjLabel) + 1);
    GLsizei labelLengthBuf = 0;
    glGetObjectLabelEXT(GL_QUERY_OBJECT_EXT, object, static_cast<GLsizei>(labelBuf.size()),
                        &labelLengthBuf, labelBuf.data());
    ASSERT_GL_NO_ERROR();

    EXPECT_EQ(static_cast<GLsizei>(strlen(kQueryObjLabel)), labelLengthBuf);
    EXPECT_STREQ(kQueryObjLabel, labelBuf.data());

    glEndQueryEXT(GL_TIME_ELAPSED_EXT);
    glDeleteQueriesEXT(1, &object);
    ASSERT_GL_NO_ERROR();

    glLabelObjectEXT(GL_QUERY_OBJECT_EXT, object, 0, kQueryObjLabel);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glGetObjectLabelEXT(GL_QUERY_OBJECT_EXT, object, static_cast<GLsizei>(labelBuf.size()),
                        &labelLengthBuf, labelBuf.data());
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Simple test for GetDebugMessageLogKHR validation
TEST_P(DebugTest, GetDebugMessageLog)
{
    ANGLE_SKIP_TEST_IF(!mDebugExtensionAvailable);

    glGetDebugMessageLogKHR(1, -1, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    EXPECT_GL_NO_ERROR();

    glGetDebugMessageLogKHR(1, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    EXPECT_GL_NO_ERROR();

    std::vector<char> messageBuf(1);
    glGetDebugMessageLogKHR(1, -1, nullptr, nullptr, nullptr, nullptr, nullptr, messageBuf.data());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glGetDebugMessageLogKHR(1, 0, nullptr, nullptr, nullptr, nullptr, nullptr, messageBuf.data());
    EXPECT_GL_NO_ERROR();
}

class DebugTestES3 : public DebugTest
{};

class DebugTestES32 : public DebugTestES3
{
    void testSetUp() override { ; }
};

struct Message
{
    GLenum source;
    GLenum type;
    GLuint id;
    GLenum severity;
    std::string message;
    const void *userParam;
};

static void GL_APIENTRY Callback(GLenum source,
                                 GLenum type,
                                 GLuint id,
                                 GLenum severity,
                                 GLsizei length,
                                 const GLchar *message,
                                 const void *userParam)
{
    Message m{source, type, id, severity, std::string(message, length), userParam};
    std::vector<Message> *messages =
        static_cast<std::vector<Message> *>(const_cast<void *>(userParam));
    messages->push_back(m);
}

// Test that all ANGLE back-ends have GL_KHR_debug enabled
TEST_P(DebugTestES3, Enabled)
{
    ASSERT_TRUE(mDebugExtensionAvailable);
}

// Test that when debug output is disabled, no message are outputted
TEST_P(DebugTestES3, DisabledOutput)
{
    ANGLE_SKIP_TEST_IF(!mDebugExtensionAvailable);

    glDisable(GL_DEBUG_OUTPUT);

    glDebugMessageInsertKHR(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, 1,
                            GL_DEBUG_SEVERITY_NOTIFICATION, -1, "discarded");

    GLint numMessages = 0;
    glGetIntegerv(GL_DEBUG_LOGGED_MESSAGES, &numMessages);
    ASSERT_EQ(0, numMessages);

    std::vector<Message> messages;
    glDebugMessageCallbackKHR(Callback, &messages);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    ASSERT_EQ(0u, messages.size());
}

// Test a basic flow of inserting a message and reading it back
TEST_P(DebugTestES3, InsertMessage)
{
    ANGLE_SKIP_TEST_IF(!mDebugExtensionAvailable);

    const GLenum source       = GL_DEBUG_SOURCE_APPLICATION;
    const GLenum type         = GL_DEBUG_TYPE_OTHER;
    const GLuint id           = 1;
    const GLenum severity     = GL_DEBUG_SEVERITY_NOTIFICATION;
    const std::string message = "Message";

    glDebugMessageInsertKHR(source, type, id, severity, -1, message.c_str());

    GLint numMessages = 0;
    glGetIntegerv(GL_DEBUG_LOGGED_MESSAGES, &numMessages);
    ASSERT_EQ(1, numMessages);

    GLint messageLength = 0;
    glGetIntegerv(GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH, &messageLength);
    EXPECT_EQ(static_cast<GLint>(message.length()) + 1, messageLength);

    GLenum sourceBuf   = 0;
    GLenum typeBuf     = 0;
    GLenum idBuf       = 0;
    GLenum severityBuf = 0;
    GLsizei lengthBuf  = 0;
    std::vector<char> messageBuf(messageLength);
    GLuint ret =
        glGetDebugMessageLogKHR(1, static_cast<GLsizei>(messageBuf.size()), &sourceBuf, &typeBuf,
                                &idBuf, &severityBuf, &lengthBuf, messageBuf.data());
    EXPECT_EQ(1u, ret);
    EXPECT_EQ(source, sourceBuf);
    EXPECT_EQ(type, typeBuf);
    EXPECT_EQ(id, idBuf);
    EXPECT_EQ(severity, severityBuf);
    EXPECT_EQ(lengthBuf, messageLength);
    EXPECT_STREQ(message.c_str(), messageBuf.data());

    glGetIntegerv(GL_DEBUG_LOGGED_MESSAGES, &numMessages);
    EXPECT_EQ(0, numMessages);

    ASSERT_GL_NO_ERROR();
}

// Test inserting multiple messages
TEST_P(DebugTestES3, InsertMessageMultiple)
{
    ANGLE_SKIP_TEST_IF(!mDebugExtensionAvailable);

    const GLenum source          = GL_DEBUG_SOURCE_APPLICATION;
    const GLenum type            = GL_DEBUG_TYPE_OTHER;
    const GLuint startID         = 1;
    const GLenum severity        = GL_DEBUG_SEVERITY_NOTIFICATION;
    const char messageRepeatChar = 'm';
    const size_t messageCount    = 32;

    for (size_t i = 0; i < messageCount; i++)
    {
        std::string message(i + 1, messageRepeatChar);
        glDebugMessageInsertKHR(source, type, startID + static_cast<GLuint>(i), severity, -1,
                                message.c_str());
    }

    GLint numMessages = 0;
    glGetIntegerv(GL_DEBUG_LOGGED_MESSAGES, &numMessages);
    ASSERT_EQ(static_cast<GLint>(messageCount), numMessages);

    for (size_t i = 0; i < messageCount; i++)
    {
        glGetIntegerv(GL_DEBUG_LOGGED_MESSAGES, &numMessages);
        EXPECT_EQ(static_cast<GLint>(messageCount - i), numMessages);

        std::string expectedMessage(i + 1, messageRepeatChar);

        GLint messageLength = 0;
        glGetIntegerv(GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH, &messageLength);
        EXPECT_EQ(static_cast<GLint>(expectedMessage.length()) + 1, messageLength);

        GLenum sourceBuf   = 0;
        GLenum typeBuf     = 0;
        GLenum idBuf       = 0;
        GLenum severityBuf = 0;
        GLsizei lengthBuf  = 0;
        std::vector<char> messageBuf(messageLength);
        GLuint ret =
            glGetDebugMessageLogKHR(1, static_cast<GLsizei>(messageBuf.size()), &sourceBuf,
                                    &typeBuf, &idBuf, &severityBuf, &lengthBuf, messageBuf.data());
        EXPECT_EQ(1u, ret);
        EXPECT_EQ(source, sourceBuf);
        EXPECT_EQ(type, typeBuf);
        EXPECT_EQ(startID + i, idBuf);
        EXPECT_EQ(severity, severityBuf);
        EXPECT_EQ(lengthBuf, messageLength);
        EXPECT_STREQ(expectedMessage.c_str(), messageBuf.data());
    }

    glGetIntegerv(GL_DEBUG_LOGGED_MESSAGES, &numMessages);
    EXPECT_EQ(0, numMessages);

    ASSERT_GL_NO_ERROR();
}

// Test that a too long label fails
TEST_P(DebugTest, ObjectLabelTooLong)
{
    ANGLE_SKIP_TEST_IF(!mDebugExtensionAvailable);

    // Limit includes the null terminator
    GLint maxLength = 0;
    glGetIntegerv(GL_MAX_LABEL_LENGTH_KHR, &maxLength);
    ASSERT_GE(maxLength, 1);

    GLBuffer object;
    glBindBuffer(GL_ARRAY_BUFFER, object);
    ASSERT_GL_NO_ERROR();

    // Implicit length
    glObjectLabelKHR(GL_BUFFER_KHR, object, -1, std::string(maxLength - 1, 'A').c_str());
    EXPECT_GL_NO_ERROR();

    glObjectLabelKHR(GL_BUFFER_KHR, object, -1, std::string(maxLength, 'A').c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glObjectLabelKHR(GL_BUFFER_KHR, object, -1, std::string(maxLength + 1, 'A').c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // Explicit length
    const std::string label = std::string(maxLength + 1, 'B');

    glObjectLabelKHR(GL_BUFFER_KHR, object, maxLength - 1, label.c_str());
    EXPECT_GL_NO_ERROR();

    glObjectLabelKHR(GL_BUFFER_KHR, object, maxLength, label.c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glObjectLabelKHR(GL_BUFFER_KHR, object, maxLength + 1, label.c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test that a too long sync object label fails
TEST_P(DebugTestES3, ObjectPtrLabelTooLong)
{
    ANGLE_SKIP_TEST_IF(!mDebugExtensionAvailable);

    // Limit includes the null terminator
    GLint maxLength = 0;
    glGetIntegerv(GL_MAX_LABEL_LENGTH_KHR, &maxLength);
    ASSERT_GE(maxLength, 1);

    GLsync object = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    ASSERT_GL_NO_ERROR();

    // Implicit length
    glObjectPtrLabelKHR(object, -1, std::string(maxLength - 1, 'A').c_str());
    EXPECT_GL_NO_ERROR();

    glObjectPtrLabelKHR(object, -1, std::string(maxLength, 'A').c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glObjectPtrLabelKHR(object, -1, std::string(maxLength + 1, 'A').c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // Explicit length
    const std::string label = std::string(maxLength + 1, 'B');

    glObjectPtrLabelKHR(object, maxLength - 1, label.c_str());
    EXPECT_GL_NO_ERROR();

    glObjectPtrLabelKHR(object, maxLength, label.c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glObjectPtrLabelKHR(object, maxLength + 1, label.c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glDeleteSync(object);
}

// Test that a too long debug group fails
TEST_P(DebugTest, PushDebugGroupTooLong)
{
    ANGLE_SKIP_TEST_IF(!mDebugExtensionAvailable);

    // Limit includes the null terminator
    GLint maxLength = 0;
    glGetIntegerv(GL_MAX_DEBUG_MESSAGE_LENGTH_KHR, &maxLength);
    ASSERT_GE(maxLength, 1);

    const GLenum source = GL_DEBUG_SOURCE_APPLICATION_KHR;

    // Implicit length
    glPushDebugGroupKHR(source, 1, -1, std::string(maxLength - 1, 'A').c_str());
    EXPECT_GL_NO_ERROR();

    glPushDebugGroupKHR(source, 1, -1, std::string(maxLength, 'A').c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glPushDebugGroupKHR(source, 1, -1, std::string(maxLength + 1, 'A').c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // Explicit length
    const std::string message = std::string(maxLength + 1, 'B');

    glPushDebugGroupKHR(source, 1, maxLength - 1, message.c_str());
    EXPECT_GL_NO_ERROR();

    glPushDebugGroupKHR(source, 1, maxLength, message.c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glPushDebugGroupKHR(source, 1, maxLength + 1, message.c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test that a too long message fails
TEST_P(DebugTest, InsertMessageTooLong)
{
    ANGLE_SKIP_TEST_IF(!mDebugExtensionAvailable);

    // Limit includes the null terminator
    GLint maxLength = 0;
    glGetIntegerv(GL_MAX_DEBUG_MESSAGE_LENGTH_KHR, &maxLength);
    ASSERT_GE(maxLength, 1);

    const GLenum source   = GL_DEBUG_SOURCE_APPLICATION_KHR;
    const GLenum type     = GL_DEBUG_TYPE_OTHER_KHR;
    const GLenum severity = GL_DEBUG_SEVERITY_NOTIFICATION_KHR;

    // Implicit length
    glDebugMessageInsertKHR(source, type, 1, severity, -1, std::string(maxLength - 1, 'A').c_str());
    EXPECT_GL_NO_ERROR();

    glDebugMessageInsertKHR(source, type, 1, severity, -1, std::string(maxLength, 'A').c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glDebugMessageInsertKHR(source, type, 1, severity, -1, std::string(maxLength + 1, 'A').c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // Explicit length
    const std::string message = std::string(maxLength + 1, 'B');

    glDebugMessageInsertKHR(source, type, 1, severity, maxLength - 1, message.c_str());
    EXPECT_GL_NO_ERROR();

    glDebugMessageInsertKHR(source, type, 1, severity, maxLength, message.c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glDebugMessageInsertKHR(source, type, 1, severity, maxLength + 1, message.c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test that passing a zero length inserts an empty message
TEST_P(DebugTest, InsertMessageZeroLength)
{
    ANGLE_SKIP_TEST_IF(!mDebugExtensionAvailable);

    GLint numMessages = 0;
    glGetIntegerv(GL_DEBUG_LOGGED_MESSAGES_KHR, &numMessages);
    ASSERT_EQ(0, numMessages);

    const GLenum source   = GL_DEBUG_SOURCE_APPLICATION_KHR;
    const GLenum type     = GL_DEBUG_TYPE_OTHER_KHR;
    const GLenum severity = GL_DEBUG_SEVERITY_NOTIFICATION_KHR;

    glDebugMessageInsertKHR(source, type, 1, severity, 0, "abc");
    EXPECT_GL_NO_ERROR();

    GLsizei lengthBuf = 0;
    std::vector<char> messageBuf(4, 0xFF);
    GLuint ret = glGetDebugMessageLogKHR(1, static_cast<GLsizei>(messageBuf.size()), nullptr,
                                         nullptr, nullptr, nullptr, &lengthBuf, messageBuf.data());
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(1u, ret);
    EXPECT_EQ(lengthBuf, 1);
    EXPECT_EQ('\x00', messageBuf[0]);
    EXPECT_EQ('\xFF', messageBuf[1]);
    EXPECT_EQ('\xFF', messageBuf[2]);
    EXPECT_EQ('\xFF', messageBuf[3]);

    glGetIntegerv(GL_DEBUG_LOGGED_MESSAGES_KHR, &numMessages);
    EXPECT_EQ(0, numMessages);
}

// Test using a debug callback
TEST_P(DebugTestES3, DebugCallback)
{
    ANGLE_SKIP_TEST_IF(!mDebugExtensionAvailable);

    std::vector<Message> messages;

    glDebugMessageCallbackKHR(Callback, &messages);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    const GLenum source       = GL_DEBUG_SOURCE_APPLICATION;
    const GLenum type         = GL_DEBUG_TYPE_OTHER;
    const GLuint id           = 1;
    const GLenum severity     = GL_DEBUG_SEVERITY_NOTIFICATION;
    const std::string message = "Message";

    glDebugMessageInsertKHR(source, type, id, severity, -1, message.c_str());

    GLint numMessages = 0;
    glGetIntegerv(GL_DEBUG_LOGGED_MESSAGES, &numMessages);
    EXPECT_EQ(0, numMessages);

    ASSERT_EQ(1u, messages.size());

    const Message &m = messages.front();
    EXPECT_EQ(source, m.source);
    EXPECT_EQ(type, m.type);
    EXPECT_EQ(id, m.id);
    EXPECT_EQ(severity, m.severity);
    EXPECT_EQ(message, m.message);

    ASSERT_GL_NO_ERROR();
}

// Test the glGetPointervKHR entry point
TEST_P(DebugTestES3, GetPointer)
{
    ANGLE_SKIP_TEST_IF(!mDebugExtensionAvailable);

    std::vector<Message> messages;

    glDebugMessageCallbackKHR(Callback, &messages);

    void *callback = nullptr;
    glGetPointervKHR(GL_DEBUG_CALLBACK_FUNCTION, &callback);
    EXPECT_EQ(reinterpret_cast<void *>(Callback), callback);

    void *userData = nullptr;
    glGetPointervKHR(GL_DEBUG_CALLBACK_USER_PARAM, &userData);
    EXPECT_EQ(static_cast<void *>(&messages), userData);
}

// Test usage of message control.  Example taken from GL_KHR_debug spec.
TEST_P(DebugTestES3, MessageControl1)
{
    ANGLE_SKIP_TEST_IF(!mDebugExtensionAvailable);

    std::vector<Message> messages;

    glDebugMessageCallbackKHR(Callback, &messages);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    // Setup of the default active debug group: Filter everything in
    glDebugMessageControlKHR(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);

    // Generate a debug marker debug output message
    glDebugMessageInsertKHR(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 100,
                            GL_DEBUG_SEVERITY_NOTIFICATION, -1, "Message 1");

    // Push debug group 1
    glPushDebugGroupKHR(GL_DEBUG_SOURCE_APPLICATION, 1, -1, "Message 2");

    // Setup of the debug group 1: Filter everything out
    glDebugMessageControlKHR(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_FALSE);

    // This message won't appear in the debug output log of
    glDebugMessageInsertKHR(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 100,
                            GL_DEBUG_SEVERITY_NOTIFICATION, -1, "Message 3");

    // Pop debug group 1, restore the volume control of the default debug group.
    glPopDebugGroupKHR();

    // Generate a debug marker debug output message
    glDebugMessageInsertKHR(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 100,
                            GL_DEBUG_SEVERITY_NOTIFICATION, -1, "Message 5");

    // Expected debug output from the GL implementation
    // Message 1
    // Message 2
    // Message 2
    // Message 5
    EXPECT_EQ(4u, messages.size());
    EXPECT_STREQ(messages[0].message.c_str(), "Message 1");
    EXPECT_STREQ(messages[1].message.c_str(), "Message 2");
    EXPECT_STREQ(messages[2].message.c_str(), "Message 2");
    EXPECT_STREQ(messages[3].message.c_str(), "Message 5");

    ASSERT_GL_NO_ERROR();
}

// Test usage of message control.  Example taken from GL_KHR_debug spec.
TEST_P(DebugTestES3, MessageControl2)
{
    ANGLE_SKIP_TEST_IF(!mDebugExtensionAvailable);

    std::vector<Message> messages;

    glDebugMessageCallbackKHR(Callback, &messages);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    // Setup the control of de debug output for the default debug group
    glDebugMessageControlKHR(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_FALSE);
    glDebugMessageControlKHR(GL_DEBUG_SOURCE_THIRD_PARTY, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr,
                             GL_FALSE);
    std::vector<GLuint> ids0 = {1234, 2345, 3456, 4567};
    glDebugMessageControlKHR(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, GL_DONT_CARE,
                             static_cast<GLuint>(ids0.size()), ids0.data(), GL_FALSE);
    glDebugMessageControlKHR(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_PORTABILITY, GL_DONT_CARE,
                             static_cast<GLuint>(ids0.size()), ids0.data(), GL_FALSE);

    // Push debug group 1
    // Inherit of the default debug group debug output volume control
    // Filtered out by glDebugMessageControl
    glPushDebugGroupKHR(GL_DEBUG_SOURCE_APPLICATION, 1, -1, "Message 1");

    // In this section of the code, we are interested in performances.
    glDebugMessageControlKHR(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_PERFORMANCE, GL_DONT_CARE,
                             0, nullptr, GL_TRUE);
    // But we already identify that some messages are not really useful for us.
    std::vector<GLuint> ids1 = {5678, 6789};
    glDebugMessageControlKHR(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, GL_DONT_CARE,
                             static_cast<GLuint>(ids1.size()), ids1.data(), GL_FALSE);

    glDebugMessageInsertKHR(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_PERFORMANCE, 1357,
                            GL_DEBUG_SEVERITY_MEDIUM, -1, "Message 2");
    glDebugMessageInsertKHR(GL_DEBUG_SOURCE_THIRD_PARTY,  // We still filter out these messages.
                            GL_DEBUG_TYPE_OTHER, 3579, GL_DEBUG_SEVERITY_MEDIUM, -1, "Message 3");

    glPopDebugGroupKHR();

    // Expected debug output from the GL implementation
    // Message 2
    EXPECT_EQ(1u, messages.size());
    EXPECT_STREQ(messages[0].message.c_str(), "Message 2");

    ASSERT_GL_NO_ERROR();
}

// Test basic usage of setting and getting labels
TEST_P(DebugTestES3, ObjectLabelsKHR)
{
    ANGLE_SKIP_TEST_IF(!mDebugExtensionAvailable);

    GLuint renderbuffer = 0;
    glGenRenderbuffers(1, &renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);

    const std::string &label = "renderbuffer";
    glObjectLabelKHR(GL_RENDERBUFFER, renderbuffer, -1, label.c_str());

    std::vector<char> labelBuf(label.length() + 1);
    GLsizei labelLengthBuf = 0;
    glGetObjectLabelKHR(GL_RENDERBUFFER, renderbuffer, static_cast<GLsizei>(labelBuf.size()),
                        &labelLengthBuf, labelBuf.data());

    EXPECT_EQ(static_cast<GLsizei>(label.length()), labelLengthBuf);
    EXPECT_STREQ(label.c_str(), labelBuf.data());

    ASSERT_GL_NO_ERROR();

    glDeleteRenderbuffers(1, &renderbuffer);

    glObjectLabelKHR(GL_RENDERBUFFER, renderbuffer, -1, label.c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glGetObjectLabelKHR(GL_RENDERBUFFER, renderbuffer, static_cast<GLsizei>(labelBuf.size()),
                        &labelLengthBuf, labelBuf.data());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test basic usage of setting and getting labels
TEST_P(DebugTestES3, ObjectPtrLabelsKHR)
{
    ANGLE_SKIP_TEST_IF(!mDebugExtensionAvailable);

    GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    const std::string &label = "sync";
    glObjectPtrLabelKHR(sync, -1, label.c_str());

    std::vector<char> labelBuf(label.length() + 1);
    GLsizei labelLengthBuf = 0;
    glGetObjectPtrLabelKHR(sync, static_cast<GLsizei>(labelBuf.size()), &labelLengthBuf,
                           labelBuf.data());

    EXPECT_EQ(static_cast<GLsizei>(label.length()), labelLengthBuf);
    EXPECT_STREQ(label.c_str(), labelBuf.data());

    ASSERT_GL_NO_ERROR();

    glDeleteSync(sync);

    glObjectPtrLabelKHR(sync, -1, label.c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glGetObjectPtrLabelKHR(sync, static_cast<GLsizei>(labelBuf.size()), &labelLengthBuf,
                           labelBuf.data());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test setting labels before, during and after rendering.  The debug markers can be validated by
// capturing this test under a graphics debugger.
TEST_P(DebugTestES3, Rendering)
{
    ANGLE_SKIP_TEST_IF(!mDebugExtensionAvailable);

    // The test produces the following hierarchy:
    //
    // Group: Before Draw
    // Message: Before Draw Marker
    //   Message: In Group 1 Marker
    //   glDrawArrays
    //   Group: After Draw 1
    //      glDrawArrays
    //      Message: In Group 2 Marker
    //
    //      glCopyTexImage <-- this breaks the render pass
    //
    //      glDrawArrays
    //   End Group
    //
    //   glCopyTexImage <-- this breaks the render pass
    //
    //   Group: After Draw 2
    //      glDrawArrays
    //
    //      glCopyTexImage <-- this breaks the render pass
    //
    //      Message: In Group 3 Marker
    //   End Group
    //   Message: After Draw Marker
    // End Group
    const std::string beforeDrawGroup = "Before Draw";
    const std::string drawGroup1      = "Group 1";
    const std::string drawGroup2      = "Group 2";

    const std::string beforeDrawMarker = "Before Draw Marker";
    const std::string inGroup1Marker   = "In Group 1 Marker";
    const std::string inGroup2Marker   = "In Group 2 Marker";
    const std::string inGroup3Marker   = "In Group 3 Marker";
    const std::string afterDrawMarker  = "After Draw Marker";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::Blue());
    glUseProgram(program);

    glPushDebugGroupKHR(GL_DEBUG_SOURCE_THIRD_PARTY, 0, -1, beforeDrawGroup.c_str());
    glDebugMessageInsertKHR(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_PERFORMANCE, 0,
                            GL_DEBUG_SEVERITY_NOTIFICATION, -1, beforeDrawMarker.c_str());
    {
        glDebugMessageInsertKHR(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
                                GL_DEBUG_SEVERITY_LOW, -1, inGroup1Marker.c_str());

        glDrawArrays(GL_TRIANGLES, 0, 6);

        glPushDebugGroupKHR(GL_DEBUG_SOURCE_APPLICATION, 0, -1, drawGroup1.c_str());
        {
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glDebugMessageInsertKHR(GL_DEBUG_SOURCE_THIRD_PARTY, GL_DEBUG_TYPE_PORTABILITY, 0,
                                    GL_DEBUG_SEVERITY_MEDIUM, -1, inGroup2Marker.c_str());

            GLTexture texture;
            glBindTexture(GL_TEXTURE_2D, texture);
            glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 1, 1, 0);

            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        glPopDebugGroupKHR();

        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 2, 2, 0);

        glPushDebugGroupKHR(GL_DEBUG_SOURCE_THIRD_PARTY, 0, -1, drawGroup2.c_str());
        {
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 3, 3, 0);

            glDebugMessageInsertKHR(GL_DEBUG_SOURCE_THIRD_PARTY, GL_DEBUG_TYPE_OTHER, 0,
                                    GL_DEBUG_SEVERITY_HIGH, -1, inGroup3Marker.c_str());
        }
        glPopDebugGroupKHR();

        glDebugMessageInsertKHR(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_ERROR, 0,
                                GL_DEBUG_SEVERITY_HIGH, -1, afterDrawMarker.c_str());
    }
    glPopDebugGroupKHR();

    ASSERT_GL_NO_ERROR();
}

// Test that glPushGroupMarker() issues a stack overflow error when the stack size is capped.
TEST_P(DebugTestES3, PushTooManyGroupMarkers)
{
    ANGLE_SKIP_TEST_IF(!mDebugExtensionAvailable);
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_debug_marker"));

    GLint maxStackDepth;
    glGetIntegerv(GL_MAX_DEBUG_GROUP_STACK_DEPTH_KHR, &maxStackDepth);

    const std::string markerLabel = "Test Group Marker";
    for (GLint i = 0; i < maxStackDepth; i++)
    {
        glPushGroupMarkerEXT(0, markerLabel.c_str());
        ASSERT_GL_NO_ERROR();
    }

    glPushGroupMarkerEXT(0, markerLabel.c_str());
    EXPECT_GL_ERROR(GL_STACK_OVERFLOW);
}

// Test that glPopGroupMarker() is ignored if the stack is already empty.
TEST_P(DebugTestES3, PopGroupMarkerWithEmptyStack)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_debug_marker"));
    glPopGroupMarkerEXT();
    ASSERT_GL_NO_ERROR();
}

// Simple test for gl[Push, Pop]DebugGroup using ES32 core APIs
TEST_P(DebugTestES32, DebugGroup)
{
    const std::string testDrawGroup = "Test Draw Group";

    // Pop without a push should generate GL_STACK_UNDERFLOW error
    glPopDebugGroup();
    EXPECT_GL_ERROR(GL_STACK_UNDERFLOW);

    // Push a test debug group and expect no error
    glPushDebugGroup(GL_DEBUG_SOURCE_THIRD_PARTY, 0, -1, testDrawGroup.c_str());
    ASSERT_GL_NO_ERROR();

    // Pop the test debug group and expect no error
    glPopDebugGroup();
    ASSERT_GL_NO_ERROR();
}

// Simple test for setting and getting labels using ES32 core APIs
TEST_P(DebugTestES32, ObjectLabels)
{
    GLuint renderbuffer = 0;
    glGenRenderbuffers(1, &renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);

    const std::string &label = "renderbuffer";
    glObjectLabel(GL_RENDERBUFFER, renderbuffer, -1, label.c_str());

    std::vector<char> labelBuf(label.length() + 1);
    GLsizei labelLengthBuf = 0;
    glGetObjectLabel(GL_RENDERBUFFER, renderbuffer, static_cast<GLsizei>(labelBuf.size()),
                     &labelLengthBuf, labelBuf.data());

    EXPECT_EQ(static_cast<GLsizei>(label.length()), labelLengthBuf);
    EXPECT_STREQ(label.c_str(), labelBuf.data());

    ASSERT_GL_NO_ERROR();

    glDeleteRenderbuffers(1, &renderbuffer);

    glObjectLabel(GL_RENDERBUFFER, renderbuffer, -1, label.c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glGetObjectLabel(GL_RENDERBUFFER, renderbuffer, static_cast<GLsizei>(labelBuf.size()),
                     &labelLengthBuf, labelBuf.data());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Simple test for setting and getting labels using ES32 core APIs
TEST_P(DebugTestES32, ObjectPtrLabels)
{
    GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    const std::string &label = "sync";
    glObjectPtrLabel(sync, -1, label.c_str());

    std::vector<char> labelBuf(label.length() + 1);
    GLsizei labelLengthBuf = 0;
    glGetObjectPtrLabel(sync, static_cast<GLsizei>(labelBuf.size()), &labelLengthBuf,
                        labelBuf.data());

    EXPECT_EQ(static_cast<GLsizei>(label.length()), labelLengthBuf);
    EXPECT_STREQ(label.c_str(), labelBuf.data());

    ASSERT_GL_NO_ERROR();

    glDeleteSync(sync);

    glObjectPtrLabel(sync, -1, label.c_str());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glGetObjectPtrLabel(sync, static_cast<GLsizei>(labelBuf.size()), &labelLengthBuf,
                        labelBuf.data());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Simple test for GetDebugMessageLog validation using ES32 core API
TEST_P(DebugTestES32, GetDebugMessageLog)
{
    glGetDebugMessageLog(1, -1, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    EXPECT_GL_NO_ERROR();

    glGetDebugMessageLog(1, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    EXPECT_GL_NO_ERROR();

    std::vector<char> messageBuf(1);
    glGetDebugMessageLog(1, -1, nullptr, nullptr, nullptr, nullptr, nullptr, messageBuf.data());
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glGetDebugMessageLog(1, 0, nullptr, nullptr, nullptr, nullptr, nullptr, messageBuf.data());
    EXPECT_GL_NO_ERROR();
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DebugTestES3);
ANGLE_INSTANTIATE_TEST_ES3(DebugTestES3);

ANGLE_INSTANTIATE_TEST(DebugTest,
                       ANGLE_ALL_TEST_PLATFORMS_ES1,
                       ANGLE_ALL_TEST_PLATFORMS_ES2,
                       ANGLE_ALL_TEST_PLATFORMS_ES3,
                       ANGLE_ALL_TEST_PLATFORMS_ES31);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DebugTestES32);
ANGLE_INSTANTIATE_TEST_ES32(DebugTestES32);
}  // namespace angle
