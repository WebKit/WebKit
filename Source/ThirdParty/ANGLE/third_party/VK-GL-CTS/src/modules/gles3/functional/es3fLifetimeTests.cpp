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
 * \brief Object lifetime tests.
 *//*--------------------------------------------------------------------*/

#include "es3fLifetimeTests.hpp"

#include "deRandom.hpp"
#include "deUniquePtr.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuSurface.hpp"
#include "gluDrawUtil.hpp"
#include "gluObjectWrapper.hpp"
#include "gluPixelTransfer.hpp"
#include "gluShaderProgram.hpp"
#include "glsLifetimeTests.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include <vector>

namespace deqp
{
namespace gles3
{
namespace Functional
{
namespace
{

using de::MovePtr;
using de::Random;
using glu::Buffer;
using glu::CallLogWrapper;
using glu::ProgramSources;
using glu::RenderContext;
using glu::VertexArray;
using std::vector;
using tcu::RenderTarget;
using tcu::Surface;
using tcu::TestContext;
using tcu::TestLog;
namespace lt = gls::LifetimeTests;
using namespace lt;
using namespace glw;
typedef TestCase::IterateResult IterateResult;

enum
{
    VIEWPORT_SIZE = 128
};

class ScaleProgram : public glu::ShaderProgram
{
public:
    ScaleProgram(lt::Context &ctx);
    void draw(GLuint vao, GLfloat scale, bool tf, Surface *dst);
    void setPos(GLuint buffer, GLuint vao);

private:
    ProgramSources getSources(void);

    const RenderContext &m_renderCtx;
    GLint m_scaleLoc;
    GLint m_posLoc;
};

enum
{
    NUM_COMPONENTS = 4,
    NUM_VERTICES   = 3
};

ScaleProgram::ScaleProgram(lt::Context &ctx)
    : glu::ShaderProgram(ctx.getRenderContext(), getSources())
    , m_renderCtx(ctx.getRenderContext())
{
    const Functions &gl = m_renderCtx.getFunctions();
    TCU_CHECK(isOk());
    m_scaleLoc = gl.getUniformLocation(getProgram(), "scale");
    m_posLoc   = gl.getAttribLocation(getProgram(), "pos");
}

#define GLSL(VERSION, BODY) ("#version " #VERSION "\n" #BODY "\n")

static const char *const s_vertexShaderSrc = GLSL(
    100, attribute vec4 pos; uniform float scale; void main() { gl_Position = vec4(scale * pos.xy, pos.zw); });

static const char *const s_fragmentShaderSrc = GLSL(
    100, void main() { gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0); });

ProgramSources ScaleProgram::getSources(void)
{
    using namespace glu;
    ProgramSources sources;
    sources << VertexSource(s_vertexShaderSrc) << FragmentSource(s_fragmentShaderSrc)
            << TransformFeedbackMode(GL_INTERLEAVED_ATTRIBS) << TransformFeedbackVarying("gl_Position");
    return sources;
}

void ScaleProgram::draw(GLuint vao, GLfloat scale, bool tf, Surface *dst)
{
    const Functions &gl = m_renderCtx.getFunctions();
    de::Random rnd(vao);
    Rectangle viewport = randomViewport(m_renderCtx, VIEWPORT_SIZE, VIEWPORT_SIZE, rnd);
    setViewport(m_renderCtx, viewport);
    gl.clearColor(0, 0, 0, 1);
    gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    gl.bindVertexArray(vao);
    gl.enableVertexAttribArray(m_posLoc);
    GLU_CHECK_CALL_ERROR(gl.useProgram(getProgram()), gl.getError());

    gl.uniform1f(m_scaleLoc, scale);

    if (tf)
    {
        gl.beginTransformFeedback(GL_TRIANGLES);
        if (gl.getError() == GL_INVALID_OPERATION)
            return;
    }
    GLU_CHECK_CALL_ERROR(gl.drawArrays(GL_TRIANGLES, 0, 3), gl.getError());
    if (tf)
        gl.endTransformFeedback();

    if (dst != nullptr)
        readRectangle(m_renderCtx, viewport, *dst);

    gl.bindVertexArray(0);
}

void ScaleProgram::setPos(GLuint buffer, GLuint vao)
{
    const Functions &gl = m_renderCtx.getFunctions();

    gl.bindBuffer(GL_ARRAY_BUFFER, buffer);
    gl.bindVertexArray(vao);
    GLU_CHECK_CALL_ERROR(gl.vertexAttribPointer(m_posLoc, NUM_COMPONENTS, GL_FLOAT, false, 0, nullptr), gl.getError());
    gl.bindVertexArray(0);
    gl.bindBuffer(GL_ARRAY_BUFFER, 0);
    GLU_CHECK_ERROR(gl.getError());
}

class VertexArrayBinder : public SimpleBinder
{
public:
    VertexArrayBinder(lt::Context &ctx) : SimpleBinder(ctx, 0, GL_NONE, GL_VERTEX_ARRAY_BINDING, true)
    {
    }
    void bind(GLuint name)
    {
        glBindVertexArray(name);
    }
};

class SamplerBinder : public Binder
{
public:
    SamplerBinder(lt::Context &ctx) : Binder(ctx)
    {
    }
    void bind(GLuint name)
    {
        glBindSampler(0, name);
    }
    GLuint getBinding(void)
    {
        GLint arr[32] = {};
        glGetIntegerv(GL_SAMPLER_BINDING, arr);
        log() << TestLog::Message << "// First output integer: " << arr[0] << TestLog::EndMessage;
        return arr[0];
    }
    bool genRequired(void) const
    {
        return true;
    }
};

class QueryBinder : public Binder
{
public:
    QueryBinder(lt::Context &ctx) : Binder(ctx)
    {
    }
    void bind(GLuint name)
    {
        if (name != 0)
            glBeginQuery(GL_ANY_SAMPLES_PASSED, name);
        else
            glEndQuery(GL_ANY_SAMPLES_PASSED);
    }
    GLuint getBinding(void)
    {
        return 0;
    }
};

class BufferVAOAttacher : public Attacher
{
public:
    BufferVAOAttacher(lt::Context &ctx, Type &elementType, Type &varrType, ScaleProgram &program)
        : Attacher(ctx, elementType, varrType)
        , m_program(program)
    {
    }
    void initAttachment(GLuint seed, GLuint element);
    void attach(GLuint element, GLuint container);
    void detach(GLuint element, GLuint container);
    bool canAttachDeleted(void) const
    {
        return false;
    }
    ScaleProgram &getProgram(void)
    {
        return m_program;
    }
    GLuint getAttachment(GLuint container);

private:
    ScaleProgram &m_program;
};

static const GLfloat s_varrData[NUM_VERTICES * NUM_COMPONENTS] = {-1.0, 0.0, 0.0, 1.0,  1.0, 1.0,
                                                                  0.0,  1.0, 0.0, -1.0, 0.0, 1.0};

void initBuffer(const Functions &gl, GLuint seed, GLenum usage, GLuint buffer)
{
    gl.bindBuffer(GL_ARRAY_BUFFER, buffer);
    if (seed == 0)
        gl.bufferData(GL_ARRAY_BUFFER, sizeof(s_varrData), s_varrData, usage);
    else
    {
        Random rnd(seed);
        GLfloat data[DE_LENGTH_OF_ARRAY(s_varrData)];

        for (int ndx = 0; ndx < NUM_VERTICES; ndx++)
        {
            GLfloat *vertex = &data[ndx * NUM_COMPONENTS];
            vertex[0]       = 2.0f * (rnd.getFloat() - 0.5f);
            vertex[1]       = 2.0f * (rnd.getFloat() - 0.5f);
            DE_STATIC_ASSERT(NUM_COMPONENTS == 4);
            vertex[2] = 0.0f;
            vertex[3] = 1.0f;
        }
        gl.bufferData(GL_ARRAY_BUFFER, sizeof(data), data, usage);
    }
    gl.bindBuffer(GL_ARRAY_BUFFER, 0);
    GLU_CHECK_ERROR(gl.getError());
}

void BufferVAOAttacher::initAttachment(GLuint seed, GLuint buffer)
{
    initBuffer(gl(), seed, GL_STATIC_DRAW, buffer);
    log() << TestLog::Message << "// Initialized buffer " << buffer << " from seed " << seed << TestLog::EndMessage;
}

void BufferVAOAttacher::attach(GLuint buffer, GLuint vao)
{
    m_program.setPos(buffer, vao);
    log() << TestLog::Message << "// Set the `pos` attribute in VAO " << vao << " to buffer " << buffer
          << TestLog::EndMessage;
}

void BufferVAOAttacher::detach(GLuint buffer, GLuint varr)
{
    DE_UNREF(buffer);
    attach(0, varr);
}

GLuint BufferVAOAttacher::getAttachment(GLuint varr)
{
    GLint name = 0;
    gl().bindVertexArray(varr);
    gl().getVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &name);
    gl().bindVertexArray(0);
    GLU_CHECK_ERROR(gl().getError());
    return GLuint(name);
}

class BufferVAOInputAttacher : public InputAttacher
{
public:
    BufferVAOInputAttacher(BufferVAOAttacher &attacher) : InputAttacher(attacher), m_program(attacher.getProgram())
    {
    }
    void drawContainer(GLuint container, Surface &dst);

private:
    ScaleProgram &m_program;
};

void BufferVAOInputAttacher::drawContainer(GLuint vao, Surface &dst)
{
    m_program.draw(vao, 1.0, false, &dst);
    log() << TestLog::Message << "// Drew an output image with VAO " << vao << TestLog::EndMessage;
}

class BufferTfAttacher : public Attacher
{
public:
    BufferTfAttacher(lt::Context &ctx, Type &bufferType, Type &tfType) : Attacher(ctx, bufferType, tfType)
    {
    }
    void initAttachment(GLuint seed, GLuint element);
    void attach(GLuint buffer, GLuint tf);
    void detach(GLuint buffer, GLuint tf);
    bool canAttachDeleted(void) const
    {
        return false;
    }
    GLuint getAttachment(GLuint tf);
};

void BufferTfAttacher::initAttachment(GLuint seed, GLuint buffer)
{
    initBuffer(gl(), seed, GL_DYNAMIC_READ, buffer);
    log() << TestLog::Message << "// Initialized buffer " << buffer << " from seed " << seed << TestLog::EndMessage;
}

void BufferTfAttacher::attach(GLuint buffer, GLuint tf)
{
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffer);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    GLU_CHECK_ERROR(gl().getError());
}

void BufferTfAttacher::detach(GLuint buffer, GLuint tf)
{
    DE_UNREF(buffer);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    GLU_CHECK_ERROR(gl().getError());
}

GLuint BufferTfAttacher::getAttachment(GLuint tf)
{
    GLint ret = 0;
    gl().bindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf);
    gl().getIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0, &ret);
    gl().bindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    GLU_CHECK_ERROR(gl().getError());
    return GLuint(ret);
}

class BufferTfOutputAttacher : public OutputAttacher
{
public:
    BufferTfOutputAttacher(BufferTfAttacher &attacher, ScaleProgram &program)
        : OutputAttacher(attacher)
        , m_program(program)
    {
    }
    void setupContainer(GLuint seed, GLuint container);
    void drawAttachment(GLuint attachment, Surface &dst);

private:
    ScaleProgram &m_program;
};

void BufferTfOutputAttacher::drawAttachment(GLuint buffer, Surface &dst)
{
    VertexArray vao(getRenderContext());

    m_program.setPos(buffer, *vao);
    m_program.draw(*vao, 1.0, false, &dst);
    log() << TestLog::Message << "// Drew output image with vertices from buffer " << buffer << TestLog::EndMessage;
    GLU_CHECK_ERROR(gl().getError());
}

void BufferTfOutputAttacher::setupContainer(GLuint seed, GLuint tf)
{
    Buffer posBuf(getRenderContext());
    VertexArray vao(getRenderContext());

    initBuffer(gl(), seed, GL_STATIC_DRAW, *posBuf);
    m_program.setPos(*posBuf, *vao);

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf);
    m_program.draw(*vao, -1.0, true, nullptr);
    log() << TestLog::Message << "// Drew an image with seed " << seed << " with transform feedback to " << tf
          << TestLog::EndMessage;
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    GLU_CHECK_ERROR(gl().getError());
}

class ES3Types : public ES2Types
{
public:
    ES3Types(lt::Context &ctx);

private:
    ScaleProgram m_program;
    QueryBinder m_queryBind;
    SimpleType m_queryType;
    SimpleBinder m_tfBind;
    SimpleType m_tfType;
    VertexArrayBinder m_varrBind;
    SimpleType m_varrType;
    SamplerBinder m_samplerBind;
    SimpleType m_samplerType;
    BufferVAOAttacher m_bufVarrAtt;
    BufferVAOInputAttacher m_bufVarrInAtt;
    BufferTfAttacher m_bufTfAtt;
    BufferTfOutputAttacher m_bufTfOutAtt;
};

ES3Types::ES3Types(lt::Context &ctx)
    : ES2Types(ctx)
    , m_program(ctx)
    , m_queryBind(ctx)
    , m_queryType(ctx, "query", &CallLogWrapper::glGenQueries, &CallLogWrapper::glDeleteQueries,
                  &CallLogWrapper::glIsQuery, &m_queryBind)
    , m_tfBind(ctx, &CallLogWrapper::glBindTransformFeedback, GL_TRANSFORM_FEEDBACK, GL_TRANSFORM_FEEDBACK_BINDING,
               true)
    , m_tfType(ctx, "transform_feedback", &CallLogWrapper::glGenTransformFeedbacks,
               &CallLogWrapper::glDeleteTransformFeedbacks, &CallLogWrapper::glIsTransformFeedback, &m_tfBind)
    , m_varrBind(ctx)
    , m_varrType(ctx, "vertex_array", &CallLogWrapper::glGenVertexArrays, &CallLogWrapper::glDeleteVertexArrays,
                 &CallLogWrapper::glIsVertexArray, &m_varrBind)
    , m_samplerBind(ctx)
    , m_samplerType(ctx, "sampler", &CallLogWrapper::glGenSamplers, &CallLogWrapper::glDeleteSamplers,
                    &CallLogWrapper::glIsSampler, &m_samplerBind, true)
    , m_bufVarrAtt(ctx, m_bufferType, m_varrType, m_program)
    , m_bufVarrInAtt(m_bufVarrAtt)
    , m_bufTfAtt(ctx, m_bufferType, m_tfType)
    , m_bufTfOutAtt(m_bufTfAtt, m_program)
{
    Type *types[] = {&m_queryType, &m_tfType, &m_varrType, &m_samplerType};
    m_types.insert(m_types.end(), DE_ARRAY_BEGIN(types), DE_ARRAY_END(types));

    m_attachers.push_back(&m_bufVarrAtt);
    m_attachers.push_back(&m_bufTfAtt);

    m_inAttachers.push_back(&m_bufVarrInAtt);
    m_outAttachers.push_back(&m_bufTfOutAtt);
}

class TfDeleteActiveTest : public TestCase, private CallLogWrapper
{
public:
    TfDeleteActiveTest(gles3::Context &context, const char *name, const char *description);
    IterateResult iterate(void);
};

TfDeleteActiveTest::TfDeleteActiveTest(gles3::Context &context, const char *name, const char *description)
    : TestCase(context, name, description)
    , CallLogWrapper(context.getRenderContext().getFunctions(), context.getTestContext().getLog())
{
    enableLogging(true);
}

class ScopedTransformFeedbackFeedback
{
public:
    ScopedTransformFeedbackFeedback(glu::CallLogWrapper &gl, GLenum type);
    ~ScopedTransformFeedbackFeedback(void);

private:
    glu::CallLogWrapper &m_gl;
};

ScopedTransformFeedbackFeedback::ScopedTransformFeedbackFeedback(glu::CallLogWrapper &gl, GLenum type) : m_gl(gl)
{
    m_gl.glBeginTransformFeedback(type);
    GLU_EXPECT_NO_ERROR(m_gl.glGetError(), "glBeginTransformFeedback");
}

ScopedTransformFeedbackFeedback::~ScopedTransformFeedbackFeedback(void)
{
    m_gl.glEndTransformFeedback();
}

IterateResult TfDeleteActiveTest::iterate(void)
{
    static const char *const s_xfbVertexSource =
        "#version 300 es\n"
        "void main ()\n"
        "{\n"
        "    gl_Position = vec4(float(gl_VertexID) / 2.0, float(gl_VertexID % 2) / 2.0, 0.0, 1.0);\n"
        "}\n";
    static const char *const s_xfbFragmentSource = "#version 300 es\n"
                                                   "layout(location=0) out mediump vec4 dEQP_FragColor;\n"
                                                   "void main ()\n"
                                                   "{\n"
                                                   "    dEQP_FragColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
                                                   "}\n";

    glu::Buffer buf(m_context.getRenderContext());
    GLuint tf = 0;
    glu::ShaderProgram program(m_context.getRenderContext(), glu::ProgramSources()
                                                                 << glu::VertexSource(s_xfbVertexSource)
                                                                 << glu::FragmentSource(s_xfbFragmentSource)
                                                                 << glu::TransformFeedbackMode(GL_INTERLEAVED_ATTRIBS)
                                                                 << glu::TransformFeedbackVarying("gl_Position"));

    if (!program.isOk())
    {
        m_testCtx.getLog() << program;
        throw tcu::TestError("failed to build program");
    }

    try
    {
        GLU_CHECK_CALL(glUseProgram(program.getProgram()));
        GLU_CHECK_CALL(glGenTransformFeedbacks(1, &tf));
        GLU_CHECK_CALL(glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf));
        GLU_CHECK_CALL(glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, *buf));
        GLU_CHECK_CALL(
            glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 3 * sizeof(glw::GLfloat[4]), nullptr, GL_DYNAMIC_COPY));

        {
            ScopedTransformFeedbackFeedback xfb(static_cast<glu::CallLogWrapper &>(*this), GL_TRIANGLES);

            glDeleteTransformFeedbacks(1, &tf);
            {
                GLenum err = glGetError();
                if (err != GL_INVALID_OPERATION)
                    getTestContext().setTestResult(
                        QP_TEST_RESULT_FAIL, "Deleting active transform feedback did not produce GL_INVALID_OPERATION");
                else
                    getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Pass");
            }
        }
        GLU_CHECK(); // ScopedTransformFeedbackFeedback::dtor might modify error state

        GLU_CHECK_CALL(glDeleteTransformFeedbacks(1, &tf));
    }
    catch (const glu::Error &)
    {
        glDeleteTransformFeedbacks(1, &tf);
        throw;
    }

    return STOP;
}

class TestGroup : public TestCaseGroup
{
public:
    TestGroup(gles3::Context &context) : TestCaseGroup(context, "lifetime", "Object lifetime tests")
    {
    }
    void init(void);

private:
    MovePtr<Types> m_types;
};

void TestGroup::init(void)
{
    gles3::Context &ctx = getContext();
    lt::Context ltCtx(ctx.getRenderContext(), ctx.getTestContext());

    m_types = MovePtr<Types>(new ES3Types(ltCtx));

    addTestCases(*this, *m_types);

    TestCaseGroup *deleteActiveGroup = new TestCaseGroup(ctx, "delete_active", "Delete active object");
    addChild(deleteActiveGroup);
    deleteActiveGroup->addChild(new TfDeleteActiveTest(ctx, "transform_feedback", "Transform Feedback"));
}

} // namespace

TestCaseGroup *createLifetimeTests(Context &context)
{
    return new TestGroup(context);
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
