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
 * \brief Depth buffer performance tests.
 *//*--------------------------------------------------------------------*/

#include "es3pDepthTests.hpp"

#include "glsCalibration.hpp"

#include "gluShaderProgram.hpp"
#include "gluObjectWrapper.hpp"
#include "gluPixelTransfer.hpp"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include "tcuTestLog.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuCPUWarmup.hpp"
#include "tcuCommandLine.hpp"
#include "tcuResultCollector.hpp"

#include "deClock.h"
#include "deString.h"
#include "deMath.h"
#include "deStringUtil.hpp"
#include "deRandom.hpp"
#include "deUniquePtr.hpp"

#include <vector>
#include <algorithm>

namespace deqp
{
namespace gles3
{
namespace Performance
{
namespace
{
using namespace glw;
using de::MovePtr;
using glu::ProgramSources;
using glu::RenderContext;
using glu::ShaderSource;
using std::map;
using std::string;
using std::vector;
using tcu::TestContext;
using tcu::TestLog;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;

struct Sample
{
    int64_t nullTime;
    int64_t baseTime;
    int64_t testTime;
    int order;
    int workload;
};

struct SampleParams
{
    int step;
    int measurement;

    SampleParams(int step_, int measurement_) : step(step_), measurement(measurement_)
    {
    }
};

typedef vector<float> Geometry;

struct ObjectData
{
    ProgramSources shader;
    Geometry geometry;

    ObjectData(const ProgramSources &shader_, const Geometry &geometry_) : shader(shader_), geometry(geometry_)
    {
    }
};

class RenderData
{
public:
    RenderData(const ObjectData &object, const glu::RenderContext &renderCtx, TestLog &log);
    ~RenderData(void)
    {
    }

    const glu::ShaderProgram m_program;
    const glu::VertexArray m_vao;
    const glu::Buffer m_vbo;

    const int m_numVertices;
};

RenderData::RenderData(const ObjectData &object, const glu::RenderContext &renderCtx, TestLog &log)
    : m_program(renderCtx, object.shader)
    , m_vao(renderCtx.getFunctions())
    , m_vbo(renderCtx.getFunctions())
    , m_numVertices(int(object.geometry.size()) / 4)
{
    const glw::Functions &gl = renderCtx.getFunctions();

    if (!m_program.isOk())
        log << m_program;

    gl.bindBuffer(GL_ARRAY_BUFFER, *m_vbo);
    gl.bufferData(GL_ARRAY_BUFFER, object.geometry.size() * sizeof(float), &object.geometry[0], GL_STATIC_DRAW);
    gl.bindAttribLocation(m_program.getProgram(), 0, "a_position");

    gl.bindVertexArray(*m_vao);
    gl.vertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    gl.enableVertexAttribArray(0);
    gl.bindVertexArray(0);
}

namespace Utils
{
vector<float> getFullscreenQuad(float depth)
{
    const float data[] = {
        +1.0f, +1.0f, depth, 0.0f, // .w is gl_VertexId%3 since Nexus 4&5 can't handle that on their own
        +1.0f, -1.0f, depth, 1.0f,  -1.0f, -1.0f, depth, 2.0f,  -1.0f, -1.0f,
        depth, 0.0f,  -1.0f, +1.0f, depth, 1.0f,  +1.0f, +1.0f, depth, 2.0f,
    };

    return vector<float>(DE_ARRAY_BEGIN(data), DE_ARRAY_END(data));
}

vector<float> getFullscreenQuadWithGradient(float depth0, float depth1)
{
    const float data[] = {
        +1.0f, +1.0f, depth0, 0.0f, +1.0f, -1.0f, depth0, 1.0f, -1.0f, -1.0f, depth1, 2.0f,
        -1.0f, -1.0f, depth1, 0.0f, -1.0f, +1.0f, depth1, 1.0f, +1.0f, +1.0f, depth0, 2.0f,
    };

    return vector<float>(DE_ARRAY_BEGIN(data), DE_ARRAY_END(data));
}

vector<float> getPartScreenQuad(float coverage, float depth)
{
    const float xMax   = -1.0f + 2.0f * coverage;
    const float data[] = {
        xMax,  +1.0f, depth, 0.0f, xMax,  -1.0f, depth, 1.0f, -1.0f, -1.0f, depth, 2.0f,
        -1.0f, -1.0f, depth, 0.0f, -1.0f, +1.0f, depth, 1.0f, xMax,  +1.0f, depth, 2.0f,
    };

    return vector<float>(DE_ARRAY_BEGIN(data), DE_ARRAY_END(data));
}

// Axis aligned grid. Depth of vertices is baseDepth +/- depthNoise
vector<float> getFullScreenGrid(int resolution, uint32_t seed, float baseDepth, float depthNoise, float xyNoise)
{
    const int gridsize = resolution + 1;
    vector<Vec3> vertices(gridsize * gridsize);
    vector<float> retval;
    de::Random rng(seed);

    for (int y = 0; y < gridsize; y++)
        for (int x = 0; x < gridsize; x++)
        {
            const bool isEdge = x == 0 || y == 0 || x == resolution || y == resolution;
            const float x_ =
                float(x) / float(resolution) * 2.0f - 1.0f + (isEdge ? 0.0f : rng.getFloat(-xyNoise, +xyNoise));
            const float y_ =
                float(y) / float(resolution) * 2.0f - 1.0f + (isEdge ? 0.0f : rng.getFloat(-xyNoise, +xyNoise));
            const float z_ = baseDepth + rng.getFloat(-depthNoise, +depthNoise);

            vertices[y * gridsize + x] = Vec3(x_, y_, z_);
        }

    retval.reserve(resolution * resolution * 6);

    for (int y = 0; y < resolution; y++)
        for (int x = 0; x < resolution; x++)
        {
            const Vec3 &p0 = vertices[(y + 0) * gridsize + (x + 0)];
            const Vec3 &p1 = vertices[(y + 0) * gridsize + (x + 1)];
            const Vec3 &p2 = vertices[(y + 1) * gridsize + (x + 0)];
            const Vec3 &p3 = vertices[(y + 1) * gridsize + (x + 1)];

            const float temp[6 * 4] = {
                p0.x(), p0.y(), p0.z(), 0.0f, p2.x(), p2.y(), p2.z(), 1.0f, p1.x(), p1.y(), p1.z(), 2.0f,

                p3.x(), p3.y(), p3.z(), 0.0f, p1.x(), p1.y(), p1.z(), 1.0f, p2.x(), p2.y(), p2.z(), 2.0f,
            };

            retval.insert(retval.end(), DE_ARRAY_BEGIN(temp), DE_ARRAY_END(temp));
        }

    return retval;
}

// Outputs barycentric coordinates as v_bcoords. Otherwise a passthrough shader
string getBaseVertexShader(void)
{
    return "#version 300 es\n"
           "in highp vec4 a_position;\n"
           "out mediump vec3 v_bcoords;\n"
           "void main()\n"
           "{\n"
           "    v_bcoords = vec3(0, 0, 0);\n"
           "    v_bcoords[int(a_position.w)] = 1.0;\n"
           "    gl_Position = vec4(a_position.xyz, 1.0);\n"
           "}\n";
}

// Adds noise to coordinates based on InstanceID Outputs barycentric coordinates as v_bcoords
string getInstanceNoiseVertexShader(void)
{
    return "#version 300 es\n"
           "in highp vec4 a_position;\n"
           "out mediump vec3 v_bcoords;\n"
           "void main()\n"
           "{\n"
           "    v_bcoords = vec3(0, 0, 0);\n"
           "    v_bcoords[int(a_position.w)] = 1.0;\n"
           "    vec3 noise = vec3(sin(float(gl_InstanceID)*1.05), sin(float(gl_InstanceID)*1.23), "
           "sin(float(gl_InstanceID)*1.71));\n"
           "    gl_Position = vec4(a_position.xyz + noise * 0.005, 1.0);\n"
           "}\n";
}

// Renders green triangles with edges highlighted. Exact shade depends on depth.
string getDepthAsGreenFragmentShader(void)
{
    return "#version 300 es\n"
           "in mediump vec3 v_bcoords;\n"
           "out mediump vec4 fragColor;\n"
           "void main()\n"
           "{\n"
           "    mediump float d = gl_FragCoord.z;\n"
           "    if (v_bcoords.x < 0.02 || v_bcoords.y < 0.02 || v_bcoords.z < 0.02)\n"
           "        fragColor = vec4(d,1,d,1);\n"
           "    else\n"
           "        fragColor = vec4(0,d,0,1);\n"
           "}\n";
}

// Renders green triangles with edges highlighted. Exact shade depends on depth.
string getDepthAsRedFragmentShader(void)
{
    return "#version 300 es\n"
           "in mediump vec3 v_bcoords;\n"
           "out mediump vec4 fragColor;\n"
           "void main()\n"
           "{\n"
           "    mediump float d = gl_FragCoord.z;\n"
           "    if (v_bcoords.x < 0.02 || v_bcoords.y < 0.02 || v_bcoords.z < 0.02)\n"
           "        fragColor = vec4(1,d,d,1);\n"
           "    else\n"
           "        fragColor = vec4(d,0,0,1);\n"
           "}\n";
}

// Basic time waster. Renders red triangles with edges highlighted. Exact shade depends on depth.
string getArithmeticWorkloadFragmentShader(void)
{

    return "#version 300 es\n"
           "in mediump vec3 v_bcoords;\n"
           "out mediump vec4 fragColor;\n"
           "uniform mediump int u_iterations;\n"
           "void main()\n"
           "{\n"
           "    mediump float d = gl_FragCoord.z;\n"
           "    for (int i = 0; i<u_iterations; i++)\n"
           // cos(a)^2 + sin(a)^2 == 1. since d is in range [0,1] this will lose a few ULP's of precision per iteration but should not significantly change the value of d without extreme iteration counts
           "        d = d*sin(d)*sin(d) + d*cos(d)*cos(d);\n"
           "    if (v_bcoords.x < 0.02 || v_bcoords.y < 0.02 || v_bcoords.z < 0.02)\n"
           "        fragColor = vec4(1,d,d,1);\n"
           "    else\n"
           "        fragColor = vec4(d,0,0,1);\n"
           "}\n";
}

// Arithmetic workload shader but contains discard
string getArithmeticWorkloadDiscardFragmentShader(void)
{
    return "#version 300 es\n"
           "in mediump vec3 v_bcoords;\n"
           "out mediump vec4 fragColor;\n"
           "uniform mediump int u_iterations;\n"
           "void main()\n"
           "{\n"
           "    mediump float d = gl_FragCoord.z;\n"
           "    for (int i = 0; i<u_iterations; i++)\n"
           "        d = d*sin(d)*sin(d) + d*cos(d)*cos(d);\n"
           "    if (d < 0.5) discard;\n"
           "    if (v_bcoords.x < 0.02 || v_bcoords.y < 0.02 || v_bcoords.z < 0.02)\n"
           "        fragColor = vec4(1,d,d,1);\n"
           "    else\n"
           "        fragColor = vec4(d,0,0,1);\n"
           "}\n";
}

// Texture fetch based time waster. Renders red triangles with edges highlighted. Exact shade depends on depth.
string getTextureWorkloadFragmentShader(void)
{
    return "#version 300 es\n"
           "in mediump vec3 v_bcoords;\n"
           "out mediump vec4 fragColor;\n"
           "uniform mediump int u_iterations;\n"
           "uniform sampler2D u_texture;\n"
           "void main()\n"
           "{\n"
           "    mediump float d = gl_FragCoord.z;\n"
           "    for (int i = 0; i<u_iterations; i++)\n"
           "        d *= texture(u_texture, (gl_FragCoord.xy+vec2(i))/512.0).r;\n" // Texture is expected to be fully white
           "    if (v_bcoords.x < 0.02 || v_bcoords.y < 0.02 || v_bcoords.z < 0.02)\n"
           "        fragColor = vec4(1,1,1,1);\n"
           "    else\n"
           "        fragColor = vec4(d,0,0,1);\n"
           "}\n";
}

// Discard fragments in a grid pattern
string getGridDiscardFragmentShader(int gridsize)
{
    const string fragSrc =
        "#version 300 es\n"
        "in mediump vec3 v_bcoords;\n"
        "out mediump vec4 fragColor;\n"
        "void main()\n"
        "{\n"
        "    mediump float d = gl_FragCoord.z;\n"
        "    if ((int(gl_FragCoord.x)/${GRIDRENDER_SIZE} + int(gl_FragCoord.y)/${GRIDRENDER_SIZE})%2 == 0)\n"
        "        discard;\n"
        "    if (v_bcoords.x < 0.02 || v_bcoords.y < 0.02 || v_bcoords.z < 0.02)\n"
        "        fragColor = vec4(d,1,d,1);\n"
        "    else\n"
        "        fragColor = vec4(0,d,0,1);\n"
        "}\n";
    map<string, string> params;

    params["GRIDRENDER_SIZE"] = de::toString(gridsize);

    return tcu::StringTemplate(fragSrc).specialize(params);
}

// A static increment to frag depth
string getStaticFragDepthFragmentShader(void)
{
    return "#version 300 es\n"
           "in mediump vec3 v_bcoords;\n"
           "out mediump vec4 fragColor;\n"
           "void main()\n"
           "{\n"
           "    mediump float d = gl_FragCoord.z;\n"
           "    gl_FragDepth = gl_FragCoord.z + 0.1;\n"
           "    if (v_bcoords.x < 0.02 || v_bcoords.y < 0.02 || v_bcoords.z < 0.02)\n"
           "        fragColor = vec4(d,1,d,1);\n"
           "    else\n"
           "        fragColor = vec4(0,d,0,1);\n"
           "}\n";
}

// A trivial dynamic change to frag depth
string getDynamicFragDepthFragmentShader(void)
{
    return "#version 300 es\n"
           "in mediump vec3 v_bcoords;\n"
           "out mediump vec4 fragColor;\n"
           "void main()\n"
           "{\n"
           "    mediump float d = gl_FragCoord.z;\n"
           "    gl_FragDepth = gl_FragCoord.z + (v_bcoords.x + v_bcoords.y + v_bcoords.z)*0.05;\n" // Sum of v_bcoords components is allways 1
           "    if (v_bcoords.x < 0.02 || v_bcoords.y < 0.02 || v_bcoords.z < 0.02)\n"
           "        fragColor = vec4(d,1,d,1);\n"
           "    else\n"
           "        fragColor = vec4(0,d,0,1);\n"
           "}\n";
}

// A static increment to frag depth
string getStaticFragDepthArithmeticWorkloadFragmentShader(void)
{
    return "#version 300 es\n"
           "in mediump vec3 v_bcoords;\n"
           "out mediump vec4 fragColor;\n"
           "uniform mediump int u_iterations;\n"
           "void main()\n"
           "{\n"
           "    mediump float d = gl_FragCoord.z;\n"
           "    gl_FragDepth = gl_FragCoord.z + 0.1;\n"
           "    for (int i = 0; i<u_iterations; i++)\n"
           "        d = d*sin(d)*sin(d) + d*cos(d)*cos(d);\n"
           "    if (v_bcoords.x < 0.02 || v_bcoords.y < 0.02 || v_bcoords.z < 0.02)\n"
           "        fragColor = vec4(1,d,d,1);\n"
           "    else\n"
           "        fragColor = vec4(d,0,0,1);\n"
           "}\n";
}

// A trivial dynamic change to frag depth
string getDynamicFragDepthArithmeticWorkloadFragmentShader(void)
{
    return "#version 300 es\n"
           "in mediump vec3 v_bcoords;\n"
           "out mediump vec4 fragColor;\n"
           "uniform mediump int u_iterations;\n"
           "void main()\n"
           "{\n"
           "    mediump float d = gl_FragCoord.z;\n"
           "    gl_FragDepth = gl_FragCoord.z + (v_bcoords.x + v_bcoords.y + v_bcoords.z)*0.05;\n" // Sum of v_bcoords components is allways 1
           "    for (int i = 0; i<u_iterations; i++)\n"
           "        d = d*sin(d)*sin(d) + d*cos(d)*cos(d);\n"
           "    if (v_bcoords.x < 0.02 || v_bcoords.y < 0.02 || v_bcoords.z < 0.02)\n"
           "        fragColor = vec4(1,d,d,1);\n"
           "    else\n"
           "        fragColor = vec4(d,0,0,1);\n"
           "}\n";
}

glu::ProgramSources getBaseShader(void)
{
    return glu::makeVtxFragSources(getBaseVertexShader(), getDepthAsGreenFragmentShader());
}

glu::ProgramSources getArithmeticWorkloadShader(void)
{
    return glu::makeVtxFragSources(getBaseVertexShader(), getArithmeticWorkloadFragmentShader());
}

glu::ProgramSources getArithmeticWorkloadDiscardShader(void)
{
    return glu::makeVtxFragSources(getBaseVertexShader(), getArithmeticWorkloadDiscardFragmentShader());
}

glu::ProgramSources getTextureWorkloadShader(void)
{
    return glu::makeVtxFragSources(getBaseVertexShader(), getTextureWorkloadFragmentShader());
}

glu::ProgramSources getGridDiscardShader(int gridsize)
{
    return glu::makeVtxFragSources(getBaseVertexShader(), getGridDiscardFragmentShader(gridsize));
}

inline ObjectData quadWith(const glu::ProgramSources &shader, float depth)
{
    return ObjectData(shader, getFullscreenQuad(depth));
}

inline ObjectData quadWith(const string &fragShader, float depth)
{
    return ObjectData(glu::makeVtxFragSources(getBaseVertexShader(), fragShader), getFullscreenQuad(depth));
}

inline ObjectData variableQuad(float depth)
{
    return ObjectData(glu::makeVtxFragSources(getInstanceNoiseVertexShader(), getDepthAsRedFragmentShader()),
                      getFullscreenQuad(depth));
}

inline ObjectData fastQuad(float depth)
{
    return ObjectData(getBaseShader(), getFullscreenQuad(depth));
}

inline ObjectData slowQuad(float depth)
{
    return ObjectData(getArithmeticWorkloadShader(), getFullscreenQuad(depth));
}

inline ObjectData fastQuadWithGradient(float depth0, float depth1)
{
    return ObjectData(getBaseShader(), getFullscreenQuadWithGradient(depth0, depth1));
}
} // namespace Utils

// Shared base
class BaseCase : public tcu::TestCase
{
public:
    enum
    {
        RENDER_SIZE = 512
    };

    BaseCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name, const char *desc);
    virtual ~BaseCase(void)
    {
    }

    virtual IterateResult iterate(void);

protected:
    void logSamples(const vector<Sample> &samples, const string &name, const string &desc);
    void logGeometry(const tcu::ConstPixelBufferAccess &sample, const glu::ShaderProgram &occluderProg,
                     const glu::ShaderProgram &occludedProg);
    virtual void logAnalysis(const vector<Sample> &samples) = 0;
    virtual void logDescription(void)                       = 0;

    virtual ObjectData genOccluderGeometry(void) const = 0;
    virtual ObjectData genOccludedGeometry(void) const = 0;

    virtual int calibrate(void) const                                                                       = 0;
    virtual Sample renderSample(const RenderData &occluder, const RenderData &occluded, int workload) const = 0;

    void render(const RenderData &data) const;
    void render(const RenderData &data, int instances) const;

    const RenderContext &m_renderCtx;
    tcu::ResultCollector m_results;

    enum
    {
        ITERATION_STEPS   = 10,
        ITERATION_SAMPLES = 16
    };
};

BaseCase::BaseCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name, const char *desc)
    : TestCase(testCtx, tcu::NODETYPE_PERFORMANCE, name, desc)
    , m_renderCtx(renderCtx)
{
}

BaseCase::IterateResult BaseCase::iterate(void)
{
    typedef de::MovePtr<RenderData> RenderDataP;

    const glw::Functions &gl = m_renderCtx.getFunctions();
    TestLog &log             = m_testCtx.getLog();

    const glu::Framebuffer framebuffer(gl);
    const glu::Renderbuffer renderbuffer(gl);
    const glu::Renderbuffer depthbuffer(gl);

    vector<Sample> results;
    vector<int> params;
    RenderDataP occluderData;
    RenderDataP occludedData;
    tcu::TextureLevel resultTex(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8),
                                RENDER_SIZE, RENDER_SIZE);
    int maxWorkload = 0;
    de::Random rng(deInt32Hash(deStringHash(getName())) ^ m_testCtx.getCommandLine().getBaseSeed());

    logDescription();

    gl.bindRenderbuffer(GL_RENDERBUFFER, *renderbuffer);
    gl.renderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, RENDER_SIZE, RENDER_SIZE);
    gl.bindRenderbuffer(GL_RENDERBUFFER, *depthbuffer);
    gl.renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, RENDER_SIZE, RENDER_SIZE);

    gl.bindFramebuffer(GL_FRAMEBUFFER, *framebuffer);
    gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, *renderbuffer);
    gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, *depthbuffer);
    gl.viewport(0, 0, RENDER_SIZE, RENDER_SIZE);
    gl.clearColor(0.125f, 0.25f, 0.5f, 1.0f);

    maxWorkload = calibrate();

    // Setup data
    occluderData = RenderDataP(new RenderData(genOccluderGeometry(), m_renderCtx, log));
    occludedData = RenderDataP(new RenderData(genOccludedGeometry(), m_renderCtx, log));

    TCU_CHECK(occluderData->m_program.isOk());
    TCU_CHECK(occludedData->m_program.isOk());

    // Force initialization of GPU resources
    gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    gl.enable(GL_DEPTH_TEST);

    render(*occluderData);
    render(*occludedData);
    glu::readPixels(m_renderCtx, 0, 0, resultTex.getAccess());

    logGeometry(resultTex.getAccess(), occluderData->m_program, occludedData->m_program);

    params.reserve(ITERATION_STEPS * ITERATION_SAMPLES);

    // Setup parameters
    for (int step = 0; step < ITERATION_STEPS; step++)
    {
        const int workload = maxWorkload * step / ITERATION_STEPS;

        for (int count = 0; count < ITERATION_SAMPLES; count++)
            params.push_back(workload);
    }

    rng.shuffle(params.begin(), params.end());

    // Render samples
    for (size_t ndx = 0; ndx < params.size(); ndx++)
    {
        const int workload = params[ndx];
        Sample sample      = renderSample(*occluderData, *occludedData, workload);

        sample.workload = workload;
        sample.order    = int(ndx);

        results.push_back(sample);
    }

    logSamples(results, "Samples", "Samples");
    logAnalysis(results);

    m_results.setTestContextResult(m_testCtx);

    return STOP;
}

void BaseCase::logSamples(const vector<Sample> &samples, const string &name, const string &desc)
{
    TestLog &log = m_testCtx.getLog();

    bool testOnly = true;

    for (size_t ndx = 0; ndx < samples.size(); ndx++)
    {
        if (samples[ndx].baseTime != 0 || samples[ndx].nullTime != 0)
        {
            testOnly = false;
            break;
        }
    }

    log << TestLog::SampleList(name, desc);

    if (testOnly)
    {
        log << TestLog::SampleInfo << TestLog::ValueInfo("Workload", "Workload", "", QP_SAMPLE_VALUE_TAG_PREDICTOR)
            << TestLog::ValueInfo("Order", "Order of sample", "", QP_SAMPLE_VALUE_TAG_PREDICTOR)
            << TestLog::ValueInfo("TestTime", "Test render time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
            << TestLog::EndSampleInfo;

        for (size_t sampleNdx = 0; sampleNdx < samples.size(); sampleNdx++)
        {
            const Sample &sample = samples[sampleNdx];

            log << TestLog::Sample << sample.workload << sample.order << sample.testTime << TestLog::EndSample;
        }
    }
    else
    {
        log << TestLog::SampleInfo << TestLog::ValueInfo("Workload", "Workload", "", QP_SAMPLE_VALUE_TAG_PREDICTOR)
            << TestLog::ValueInfo("Order", "Order of sample", "", QP_SAMPLE_VALUE_TAG_PREDICTOR)
            << TestLog::ValueInfo("TestTime", "Test render time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
            << TestLog::ValueInfo("NullTime", "Read pixels time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
            << TestLog::ValueInfo("BaseTime", "Base render time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
            << TestLog::EndSampleInfo;

        for (size_t sampleNdx = 0; sampleNdx < samples.size(); sampleNdx++)
        {
            const Sample &sample = samples[sampleNdx];

            log << TestLog::Sample << sample.workload << sample.order << sample.testTime << sample.nullTime
                << sample.baseTime << TestLog::EndSample;
        }
    }

    log << TestLog::EndSampleList;
}

void BaseCase::logGeometry(const tcu::ConstPixelBufferAccess &sample, const glu::ShaderProgram &occluderProg,
                           const glu::ShaderProgram &occludedProg)
{
    TestLog &log = m_testCtx.getLog();

    log << TestLog::Section("Geometry", "Geometry");
    log << TestLog::Message << "Occluding geometry is green with shade dependent on depth (rgb == 0, depth, 0)"
        << TestLog::EndMessage;
    log << TestLog::Message << "Occluded geometry is red with shade dependent on depth (rgb == depth, 0, 0)"
        << TestLog::EndMessage;
    log << TestLog::Message << "Primitive edges are a lighter shade of red/green" << TestLog::EndMessage;

    log << TestLog::Image("Test Geometry", "Test Geometry", sample);
    log << TestLog::EndSection;

    log << TestLog::Section("Occluder", "Occluder");
    log << occluderProg;
    log << TestLog::EndSection;

    log << TestLog::Section("Occluded", "Occluded");
    log << occludedProg;
    log << TestLog::EndSection;
}

void BaseCase::render(const RenderData &data) const
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    gl.useProgram(data.m_program.getProgram());

    gl.bindVertexArray(*data.m_vao);
    gl.drawArrays(GL_TRIANGLES, 0, data.m_numVertices);
    gl.bindVertexArray(0);
}

void BaseCase::render(const RenderData &data, int instances) const
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    gl.useProgram(data.m_program.getProgram());

    gl.bindVertexArray(*data.m_vao);
    gl.drawArraysInstanced(GL_TRIANGLES, 0, data.m_numVertices, instances);
    gl.bindVertexArray(0);
}

// Render occluder once, then repeatedly render occluded geometry. Sample with multiple repetition counts & establish time per call with linear regression
class RenderCountCase : public BaseCase
{
public:
    RenderCountCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name, const char *desc);
    ~RenderCountCase(void)
    {
    }

protected:
    virtual void logAnalysis(const vector<Sample> &samples);

private:
    virtual int calibrate(void) const;
    virtual Sample renderSample(const RenderData &occluder, const RenderData &occluded, int callcount) const;
};

RenderCountCase::RenderCountCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name,
                                 const char *desc)
    : BaseCase(testCtx, renderCtx, name, desc)
{
}

void RenderCountCase::logAnalysis(const vector<Sample> &samples)
{
    using namespace gls;

    TestLog &log    = m_testCtx.getLog();
    int maxWorkload = 0;
    vector<Vec2> testSamples(samples.size());

    for (size_t ndx = 0; ndx < samples.size(); ndx++)
    {
        const Sample &sample = samples[ndx];

        testSamples[ndx] = Vec2((float)sample.workload, (float)sample.testTime);

        maxWorkload = de::max(maxWorkload, sample.workload);
    }

    {
        const float confidence                       = 0.60f;
        const LineParametersWithConfidence testParam = theilSenSiegelLinearRegression(testSamples, confidence);
        const float usPerCall                        = testParam.coefficient;
        const float pxPerCall                        = RENDER_SIZE * RENDER_SIZE;
        const float pxPerUs                          = pxPerCall / usPerCall;
        const float mpxPerS                          = pxPerUs;

        log << TestLog::Section("Linear Regression", "Linear Regression");
        log << TestLog::Message
            << "Offset & coefficient presented as [confidence interval min, estimate, confidence interval max]. "
               "Reported confidence interval for this test is "
            << confidence << TestLog::EndMessage;
        log << TestLog::Message << "Render time for scene with depth test was\n\t"
            << "[" << testParam.offsetConfidenceLower << ", " << testParam.offset << ", "
            << testParam.offsetConfidenceUpper << "]us +"
            << "[" << testParam.coefficientConfidenceLower << ", " << testParam.coefficient << ", "
            << testParam.coefficientConfidenceUpper << "]"
            << "us/workload" << TestLog::EndMessage;
        log << TestLog::EndSection;

        log << TestLog::Section("Result", "Result");

        if (testParam.coefficientConfidenceLower < 0.0f)
        {
            log << TestLog::Message
                << "Coefficient confidence bounds include values below 0.0, the operation likely has neglible "
                   "per-pixel cost"
                << TestLog::EndMessage;
            m_results.addResult(QP_TEST_RESULT_PASS, "Pass");
        }
        else if (testParam.coefficientConfidenceLower < testParam.coefficientConfidenceUpper * 0.25)
        {
            log << TestLog::Message << "Coefficient confidence range is extremely large, cannot give reliable result"
                << TestLog::EndMessage;
            m_results.addResult(QP_TEST_RESULT_PASS, "Result confidence extremely low");
        }
        else
        {
            log << TestLog::Message << "Culled hidden pixels @ " << mpxPerS << "Mpx/s" << TestLog::EndMessage;
            m_results.addResult(QP_TEST_RESULT_PASS, de::floatToString(mpxPerS, 2));
        }

        log << TestLog::EndSection;
    }
}

Sample RenderCountCase::renderSample(const RenderData &occluder, const RenderData &occluded, int callcount) const
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    Sample sample;
    uint64_t now  = 0;
    uint64_t prev = 0;
    uint8_t buffer[4];

    // Stabilize
    {
        gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        gl.enable(GL_DEPTH_TEST);
        gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    }

    prev = deGetMicroseconds();

    gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    gl.enable(GL_DEPTH_TEST);

    render(occluder);
    render(occluded, callcount);

    gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

    now = deGetMicroseconds();

    sample.testTime = now - prev;
    sample.baseTime = 0;
    sample.nullTime = 0;
    sample.workload = callcount;

    return sample;
}

int RenderCountCase::calibrate(void) const
{
    using namespace gls;

    const glw::Functions &gl = m_renderCtx.getFunctions();
    TestLog &log             = m_testCtx.getLog();

    const RenderData occluderGeometry(genOccluderGeometry(), m_renderCtx, log);
    const RenderData occludedGeometry(genOccludedGeometry(), m_renderCtx, log);

    TheilSenCalibrator calibrator(CalibratorParameters(20,     // Initial workload
                                                       10,     // Max iteration frames
                                                       20.0f,  // Iteration shortcut threshold ms
                                                       20,     // Max iterations
                                                       33.0f,  // Target frame time
                                                       40.0f,  // Frame time cap
                                                       1000.0f // Target measurement duration
                                                       ));

    while (true)
    {
        switch (calibrator.getState())
        {
        case TheilSenCalibrator::STATE_FINISHED:
            logCalibrationInfo(m_testCtx.getLog(), calibrator);
            return calibrator.getCallCount();

        case TheilSenCalibrator::STATE_MEASURE:
        {
            uint8_t buffer[4];
            int64_t now;
            int64_t prev;

            prev = deGetMicroseconds();

            gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            gl.disable(GL_DEPTH_TEST);

            render(occluderGeometry);
            render(occludedGeometry, calibrator.getCallCount());

            gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

            now = deGetMicroseconds();

            calibrator.recordIteration(now - prev);
            break;
        }

        case TheilSenCalibrator::STATE_RECOMPUTE_PARAMS:
            calibrator.recomputeParameters();
            break;
        default:
            DE_ASSERT(false);
            return 1;
        }
    }
}

// Compares time/workload gradients of same geometry with and without depth testing
class RelativeChangeCase : public BaseCase
{
public:
    RelativeChangeCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name, const char *desc);
    virtual ~RelativeChangeCase(void)
    {
    }

protected:
    Sample renderSample(const RenderData &occluder, const RenderData &occluded, int workload) const;

    virtual void logAnalysis(const vector<Sample> &samples);

private:
    int calibrate(void) const;
};

RelativeChangeCase::RelativeChangeCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name,
                                       const char *desc)
    : BaseCase(testCtx, renderCtx, name, desc)
{
}

int RelativeChangeCase::calibrate(void) const
{
    using namespace gls;

    const glw::Functions &gl = m_renderCtx.getFunctions();
    TestLog &log             = m_testCtx.getLog();

    const RenderData geom(genOccludedGeometry(), m_renderCtx, log);

    TheilSenCalibrator calibrator(CalibratorParameters(20,     // Initial workload
                                                       10,     // Max iteration frames
                                                       20.0f,  // Iteration shortcut threshold ms
                                                       20,     // Max iterations
                                                       33.0f,  // Target frame time
                                                       40.0f,  // Frame time cap
                                                       1000.0f // Target measurement duration
                                                       ));

    while (true)
    {
        switch (calibrator.getState())
        {
        case TheilSenCalibrator::STATE_FINISHED:
            logCalibrationInfo(m_testCtx.getLog(), calibrator);
            return calibrator.getCallCount();

        case TheilSenCalibrator::STATE_MEASURE:
        {
            uint8_t buffer[4];
            const GLuint program = geom.m_program.getProgram();

            gl.useProgram(program);
            gl.uniform1i(gl.getUniformLocation(program, "u_iterations"), calibrator.getCallCount());

            const int64_t prev = deGetMicroseconds();

            gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            gl.disable(GL_DEPTH_TEST);

            render(geom);

            gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

            const int64_t now = deGetMicroseconds();

            calibrator.recordIteration(now - prev);
            break;
        }

        case TheilSenCalibrator::STATE_RECOMPUTE_PARAMS:
            calibrator.recomputeParameters();
            break;
        default:
            DE_ASSERT(false);
            return 1;
        }
    }
}

Sample RelativeChangeCase::renderSample(const RenderData &occluder, const RenderData &occluded, int workload) const
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    const GLuint program     = occluded.m_program.getProgram();
    Sample sample;
    uint64_t now  = 0;
    uint64_t prev = 0;
    uint8_t buffer[4];

    gl.useProgram(program);
    gl.uniform1i(gl.getUniformLocation(program, "u_iterations"), workload);

    // Warmup (this workload seems to reduce variation in following workloads)
    {
        gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        gl.disable(GL_DEPTH_TEST);

        gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    }

    // Null time
    {
        prev = deGetMicroseconds();

        gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        gl.disable(GL_DEPTH_TEST);

        gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

        now = deGetMicroseconds();

        sample.nullTime = now - prev;
    }

    // Test time
    {
        prev = deGetMicroseconds();

        gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        gl.enable(GL_DEPTH_TEST);

        render(occluder);
        render(occluded);

        gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

        now = deGetMicroseconds();

        sample.testTime = now - prev;
    }

    // Base time
    {
        prev = deGetMicroseconds();

        gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        gl.disable(GL_DEPTH_TEST);

        render(occluder);
        render(occluded);

        gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

        now = deGetMicroseconds();

        sample.baseTime = now - prev;
    }

    sample.workload = 0;

    return sample;
}

void RelativeChangeCase::logAnalysis(const vector<Sample> &samples)
{
    using namespace gls;

    TestLog &log = m_testCtx.getLog();

    int maxWorkload = 0;

    vector<Vec2> nullSamples(samples.size());
    vector<Vec2> baseSamples(samples.size());
    vector<Vec2> testSamples(samples.size());

    for (size_t ndx = 0; ndx < samples.size(); ndx++)
    {
        const Sample &sample = samples[ndx];

        nullSamples[ndx] = Vec2((float)sample.workload, (float)sample.nullTime);
        baseSamples[ndx] = Vec2((float)sample.workload, (float)sample.baseTime);
        testSamples[ndx] = Vec2((float)sample.workload, (float)sample.testTime);

        maxWorkload = de::max(maxWorkload, sample.workload);
    }

    {
        const float confidence = 0.60f;

        const LineParametersWithConfidence nullParam = theilSenSiegelLinearRegression(nullSamples, confidence);
        const LineParametersWithConfidence baseParam = theilSenSiegelLinearRegression(baseSamples, confidence);
        const LineParametersWithConfidence testParam = theilSenSiegelLinearRegression(testSamples, confidence);

        if (!de::inRange(0.0f, nullParam.coefficientConfidenceLower, nullParam.coefficientConfidenceUpper))
        {
            m_results.addResult(QP_TEST_RESULT_FAIL, "Constant operation sequence duration not constant");
            log << TestLog::Message
                << "Constant operation sequence timing may vary as a function of workload. Result quality extremely low"
                << TestLog::EndMessage;
        }

        if (de::inRange(0.0f, baseParam.coefficientConfidenceLower, baseParam.coefficientConfidenceUpper))
        {
            m_results.addResult(QP_TEST_RESULT_FAIL, "Workload has no effect on duration");
            log << TestLog::Message << "Workload factor has no effect on duration of sample (smart optimizer?)"
                << TestLog::EndMessage;
        }

        log << TestLog::Section("Linear Regression", "Linear Regression");
        log << TestLog::Message
            << "Offset & coefficient presented as [confidence interval min, estimate, confidence interval max]. "
               "Reported confidence interval for this test is "
            << confidence << TestLog::EndMessage;

        log << TestLog::Message << "Render time for empty scene was\n\t"
            << "[" << nullParam.offsetConfidenceLower << ", " << nullParam.offset << ", "
            << nullParam.offsetConfidenceUpper << "]us +"
            << "[" << nullParam.coefficientConfidenceLower << ", " << nullParam.coefficient << ", "
            << nullParam.coefficientConfidenceUpper << "]"
            << "us/workload" << TestLog::EndMessage;

        log << TestLog::Message << "Render time for scene without depth test was\n\t"
            << "[" << baseParam.offsetConfidenceLower << ", " << baseParam.offset << ", "
            << baseParam.offsetConfidenceUpper << "]us +"
            << "[" << baseParam.coefficientConfidenceLower << ", " << baseParam.coefficient << ", "
            << baseParam.coefficientConfidenceUpper << "]"
            << "us/workload" << TestLog::EndMessage;

        log << TestLog::Message << "Render time for scene with depth test was\n\t"
            << "[" << testParam.offsetConfidenceLower << ", " << testParam.offset << ", "
            << testParam.offsetConfidenceUpper << "]us +"
            << "[" << testParam.coefficientConfidenceLower << ", " << testParam.coefficient << ", "
            << testParam.coefficientConfidenceUpper << "]"
            << "us/workload" << TestLog::EndMessage;

        log << TestLog::EndSection;

        if (de::inRange(0.0f, testParam.coefficientConfidenceLower, testParam.coefficientConfidenceUpper))
        {
            log << TestLog::Message << "Test duration not dependent on culled workload" << TestLog::EndMessage;
            m_results.addResult(QP_TEST_RESULT_PASS, "0.0");
        }
        else if (testParam.coefficientConfidenceLower < testParam.coefficientConfidenceUpper * 0.25)
        {
            log << TestLog::Message << "Coefficient confidence range is extremely large, cannot give reliable result"
                << TestLog::EndMessage;
            m_results.addResult(QP_TEST_RESULT_PASS, "Result confidence extremely low");
        }
        else if (baseParam.coefficientConfidenceLower < baseParam.coefficientConfidenceUpper * 0.25)
        {
            log << TestLog::Message
                << "Coefficient confidence range for base render time is extremely large, cannot give reliable result"
                << TestLog::EndMessage;
            m_results.addResult(QP_TEST_RESULT_PASS, "Result confidence extremely low");
        }
        else
        {
            log << TestLog::Message << "Test duration is dependent on culled workload" << TestLog::EndMessage;
            m_results.addResult(QP_TEST_RESULT_PASS,
                                de::floatToString(de::abs(testParam.coefficient) / de::abs(baseParam.coefficient), 2));
        }
    }
}

// Speed of trivial culling
class BaseCostCase : public RenderCountCase
{
public:
    BaseCostCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name, const char *desc)
        : RenderCountCase(testCtx, renderCtx, name, desc)
    {
    }

    ~BaseCostCase(void)
    {
    }

private:
    virtual ObjectData genOccluderGeometry(void) const
    {
        return Utils::fastQuad(0.2f);
    }
    virtual ObjectData genOccludedGeometry(void) const
    {
        return Utils::variableQuad(0.8f);
    }

    virtual void logDescription(void)
    {
        TestLog &log = m_testCtx.getLog();

        log << TestLog::Section("Description", "Test description");
        log << TestLog::Message << "Testing hidden fragment culling speed" << TestLog::EndMessage;
        log << TestLog::Message
            << "Geometry consists of two fullsceen quads. The first (occluding) is rendered once, the second "
               "(occluded) is rendered repeatedly"
            << TestLog::EndMessage;
        log << TestLog::Message << "Workload indicates the number of times the occluded quad is rendered"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "The time per culled pixel is estimated from the rate of change of rendering time as a function of "
               "workload"
            << TestLog::EndMessage;
        log << TestLog::EndSection;
    }
};

// Gradient
class GradientCostCase : public RenderCountCase
{
public:
    GradientCostCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name, const char *desc,
                     float gradientDistance)
        : RenderCountCase(testCtx, renderCtx, name, desc)
        , m_gradientDistance(gradientDistance)
    {
    }

    ~GradientCostCase(void)
    {
    }

private:
    virtual ObjectData genOccluderGeometry(void) const
    {
        return Utils::fastQuadWithGradient(0.0f, 1.0f - m_gradientDistance);
    }
    virtual ObjectData genOccludedGeometry(void) const
    {
        return ObjectData(
            glu::makeVtxFragSources(Utils::getInstanceNoiseVertexShader(), Utils::getDepthAsRedFragmentShader()),
            Utils::getFullscreenQuadWithGradient(m_gradientDistance, 1.0f));
    }

    virtual void logDescription(void)
    {
        TestLog &log = m_testCtx.getLog();

        log << TestLog::Section("Description", "Test description");
        log << TestLog::Message << "Testing hidden fragment culling speed" << TestLog::EndMessage;
        log << TestLog::Message
            << "Geometry consists of two fullsceen quads. The first (occluding) is rendered once, the second "
               "(occluded) is rendered repeatedly"
            << TestLog::EndMessage;
        log << TestLog::Message << "Workload indicates the number of times the occluded quad is rendered"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "The quads are tilted so that the left edge of the occluded quad has a depth of 1.0 and the right edge "
               "of the occluding quad has a depth of 0.0."
            << TestLog::EndMessage;
        log << TestLog::Message << "The quads are spaced to have a depth difference of " << m_gradientDistance
            << " at all points." << TestLog::EndMessage;
        log << TestLog::Message
            << "The time per culled pixel is estimated from the rate of change of rendering time as a function of "
               "workload"
            << TestLog::EndMessage;
        log << TestLog::EndSection;
    }

    const float m_gradientDistance;
};

// Constant offset to frag depth in occluder
class OccluderStaticFragDepthCostCase : public RenderCountCase
{
public:
    OccluderStaticFragDepthCostCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name,
                                    const char *desc)
        : RenderCountCase(testCtx, renderCtx, name, desc)
    {
    }

    ~OccluderStaticFragDepthCostCase(void)
    {
    }

private:
    virtual ObjectData genOccluderGeometry(void) const
    {
        return Utils::quadWith(Utils::getStaticFragDepthFragmentShader(), 0.2f);
    }
    virtual ObjectData genOccludedGeometry(void) const
    {
        return Utils::fastQuad(0.8f);
    }

    virtual void logDescription(void)
    {
        TestLog &log = m_testCtx.getLog();

        log << TestLog::Section("Description", "Test description");
        log << TestLog::Message << "Testing hidden fragment culling speed" << TestLog::EndMessage;
        log << TestLog::Message
            << "Geometry consists of two fullsceen quads. The first (occluding) is rendered once, the second "
               "(occluded) is rendered repeatedly"
            << TestLog::EndMessage;
        log << TestLog::Message << "Workload indicates the number of times the occluded quad is rendered"
            << TestLog::EndMessage;
        log << TestLog::Message << "The occluder quad has a static offset applied to gl_FragDepth"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "The time per culled pixel is estimated from the rate of change of rendering time as a function of "
               "workload"
            << TestLog::EndMessage;
        log << TestLog::EndSection;
    }
};

// Dynamic offset to frag depth in occluder
class OccluderDynamicFragDepthCostCase : public RenderCountCase
{
public:
    OccluderDynamicFragDepthCostCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name,
                                     const char *desc)
        : RenderCountCase(testCtx, renderCtx, name, desc)
    {
    }

    ~OccluderDynamicFragDepthCostCase(void)
    {
    }

private:
    virtual ObjectData genOccluderGeometry(void) const
    {
        return Utils::quadWith(Utils::getDynamicFragDepthFragmentShader(), 0.2f);
    }
    virtual ObjectData genOccludedGeometry(void) const
    {
        return Utils::fastQuad(0.8f);
    }

    virtual void logDescription(void)
    {
        TestLog &log = m_testCtx.getLog();

        log << TestLog::Section("Description", "Test description");
        log << TestLog::Message << "Testing hidden fragment culling speed" << TestLog::EndMessage;
        log << TestLog::Message
            << "Geometry consists of two fullsceen quads. The first (occluding) is rendered once, the second "
               "(occluded) is rendered repeatedly"
            << TestLog::EndMessage;
        log << TestLog::Message << "Workload indicates the number of times the occluded quad is rendered"
            << TestLog::EndMessage;
        log << TestLog::Message << "The occluder quad has a dynamic offset applied to gl_FragDepth"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "The time per culled pixel is estimated from the rate of change of rendering time as a function of "
               "workload"
            << TestLog::EndMessage;
        log << TestLog::EndSection;
    }
};

// Constant offset to frag depth in occluder
class OccludedStaticFragDepthCostCase : public RenderCountCase
{
public:
    OccludedStaticFragDepthCostCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name,
                                    const char *desc)
        : RenderCountCase(testCtx, renderCtx, name, desc)
    {
    }

    ~OccludedStaticFragDepthCostCase(void)
    {
    }

private:
    virtual ObjectData genOccluderGeometry(void) const
    {
        return Utils::fastQuad(0.2f);
    }
    virtual ObjectData genOccludedGeometry(void) const
    {
        return Utils::quadWith(Utils::getStaticFragDepthFragmentShader(), 0.2f);
    }

    virtual void logDescription(void)
    {
        TestLog &log = m_testCtx.getLog();

        log << TestLog::Section("Description", "Test description");
        log << TestLog::Message << "Testing hidden fragment culling speed" << TestLog::EndMessage;
        log << TestLog::Message
            << "Geometry consists of two fullsceen quads. The first (occluding) is rendered once, the second "
               "(occluded) is rendered repeatedly"
            << TestLog::EndMessage;
        log << TestLog::Message << "Workload indicates the number of times the occluded quad is rendered"
            << TestLog::EndMessage;
        log << TestLog::Message << "The occluded quad has a static offset applied to gl_FragDepth"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "The time per culled pixel is estimated from the rate of change of rendering time as a function of "
               "workload"
            << TestLog::EndMessage;
        log << TestLog::EndSection;
    }
};

// Dynamic offset to frag depth in occluder
class OccludedDynamicFragDepthCostCase : public RenderCountCase
{
public:
    OccludedDynamicFragDepthCostCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name,
                                     const char *desc)
        : RenderCountCase(testCtx, renderCtx, name, desc)
    {
    }

    ~OccludedDynamicFragDepthCostCase(void)
    {
    }

private:
    virtual ObjectData genOccluderGeometry(void) const
    {
        return Utils::fastQuad(0.2f);
    }
    virtual ObjectData genOccludedGeometry(void) const
    {
        return Utils::quadWith(Utils::getDynamicFragDepthFragmentShader(), 0.2f);
    }

    virtual void logDescription(void)
    {
        TestLog &log = m_testCtx.getLog();

        log << TestLog::Section("Description", "Test description");
        log << TestLog::Message << "Testing hidden fragment culling speed" << TestLog::EndMessage;
        log << TestLog::Message
            << "Geometry consists of two fullsceen quads. The first (occluding) is rendered once, the second "
               "(occluded) is rendered repeatedly"
            << TestLog::EndMessage;
        log << TestLog::Message << "Workload indicates the number of times the occluded quad is rendered"
            << TestLog::EndMessage;
        log << TestLog::Message << "The occluded quad has a dynamic offset applied to gl_FragDepth"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "The time per culled pixel is estimated from the rate of change of rendering time as a function of "
               "workload"
            << TestLog::EndMessage;
        log << TestLog::EndSection;
    }
};

// Culling speed with slightly less trivial geometry
class OccludingGeometryComplexityCostCase : public RenderCountCase
{
public:
    OccludingGeometryComplexityCostCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name,
                                        const char *desc, int resolution, float xyNoise, float zNoise)
        : RenderCountCase(testCtx, renderCtx, name, desc)
        , m_resolution(resolution)
        , m_xyNoise(xyNoise)
        , m_zNoise(zNoise)
    {
    }

    ~OccludingGeometryComplexityCostCase(void)
    {
    }

private:
    virtual ObjectData genOccluderGeometry(void) const
    {
        return ObjectData(Utils::getBaseShader(), Utils::getFullScreenGrid(m_resolution,
                                                                           deInt32Hash(deStringHash(getName())) ^
                                                                               m_testCtx.getCommandLine().getBaseSeed(),
                                                                           0.2f, m_zNoise, m_xyNoise));
    }

    virtual ObjectData genOccludedGeometry(void) const
    {
        return Utils::variableQuad(0.8f);
    }

    virtual void logDescription(void)
    {
        TestLog &log = m_testCtx.getLog();

        log << TestLog::Section("Description", "Test description");
        log << TestLog::Message << "Testing hidden fragment culling speed" << TestLog::EndMessage;
        log << TestLog::Message
            << "Geometry consists of an occluding grid and an occluded fullsceen quad. The occluding geometry is "
               "rendered once, the occluded one is rendered repeatedly"
            << TestLog::EndMessage;
        log << TestLog::Message << "Workload indicates the number of times the occluded quad is rendered"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "The time per culled pixel is estimated from the rate of change of rendering time as a function of "
               "workload"
            << TestLog::EndMessage;
        log << TestLog::EndSection;
    }

    const int m_resolution;
    const float m_xyNoise;
    const float m_zNoise;
};

// Cases with varying workloads in the fragment shader
class FragmentWorkloadCullCase : public RelativeChangeCase
{
public:
    FragmentWorkloadCullCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name, const char *desc);
    virtual ~FragmentWorkloadCullCase(void)
    {
    }

private:
    virtual ObjectData genOccluderGeometry(void) const
    {
        return Utils::fastQuad(0.2f);
    }

    virtual void logDescription(void);
};

FragmentWorkloadCullCase::FragmentWorkloadCullCase(TestContext &testCtx, const RenderContext &renderCtx,
                                                   const char *name, const char *desc)
    : RelativeChangeCase(testCtx, renderCtx, name, desc)
{
}

void FragmentWorkloadCullCase::logDescription(void)
{
    TestLog &log = m_testCtx.getLog();

    log << TestLog::Section("Description", "Test description");
    log << TestLog::Message << "Testing effects of culled fragment workload on render time" << TestLog::EndMessage;
    log << TestLog::Message
        << "Geometry consists of two fullsceen quads. The first (occluding) quad uses a trivial shader,"
           "the second (occluded) contains significant fragment shader work"
        << TestLog::EndMessage;
    log << TestLog::Message
        << "Workload indicates the number of iterations of unused work done in the occluded quad's fragment shader"
        << TestLog::EndMessage;
    log << TestLog::Message << "The ratio of rendering times of this scene with/without depth testing are compared"
        << TestLog::EndMessage;
    log << TestLog::Message
        << "Successfull early Z-testing should result in no correlation between workload and render time"
        << TestLog::EndMessage;
    log << TestLog::EndSection;
}

// Additional workload consists of texture lookups
class FragmentTextureWorkloadCullCase : public FragmentWorkloadCullCase
{
public:
    FragmentTextureWorkloadCullCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name,
                                    const char *desc);
    virtual ~FragmentTextureWorkloadCullCase(void)
    {
    }

    virtual void init(void);
    virtual void deinit(void);

private:
    typedef MovePtr<glu::Texture> TexPtr;

    virtual ObjectData genOccludedGeometry(void) const
    {
        return ObjectData(Utils::getTextureWorkloadShader(), Utils::getFullscreenQuad(0.8f));
    }

    TexPtr m_texture;
};

FragmentTextureWorkloadCullCase::FragmentTextureWorkloadCullCase(TestContext &testCtx, const RenderContext &renderCtx,
                                                                 const char *name, const char *desc)
    : FragmentWorkloadCullCase(testCtx, renderCtx, name, desc)
{
}

void FragmentTextureWorkloadCullCase::init(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    const int size           = 128;
    const vector<uint8_t> data(size * size * 4, 255);

    m_texture = MovePtr<glu::Texture>(new glu::Texture(gl));

    gl.bindTexture(GL_TEXTURE_2D, m_texture);
    gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data[0]);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void FragmentTextureWorkloadCullCase::deinit(void)
{
    m_texture.clear();
}

// Additional workload consists of arithmetic
class FragmentArithmeticWorkloadCullCase : public FragmentWorkloadCullCase
{
public:
    FragmentArithmeticWorkloadCullCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name,
                                       const char *desc)
        : FragmentWorkloadCullCase(testCtx, renderCtx, name, desc)
    {
    }
    virtual ~FragmentArithmeticWorkloadCullCase(void)
    {
    }

private:
    virtual ObjectData genOccludedGeometry(void) const
    {
        return ObjectData(Utils::getArithmeticWorkloadShader(), Utils::getFullscreenQuad(0.8f));
    }
};

// Contains dynamicly unused discard after a series of calculations
class FragmentDiscardArithmeticWorkloadCullCase : public FragmentWorkloadCullCase
{
public:
    FragmentDiscardArithmeticWorkloadCullCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name,
                                              const char *desc)
        : FragmentWorkloadCullCase(testCtx, renderCtx, name, desc)
    {
    }

    virtual ~FragmentDiscardArithmeticWorkloadCullCase(void)
    {
    }

private:
    virtual ObjectData genOccludedGeometry(void) const
    {
        return ObjectData(Utils::getArithmeticWorkloadDiscardShader(), Utils::getFullscreenQuad(0.8f));
    }

    virtual void logDescription(void)
    {
        TestLog &log = m_testCtx.getLog();

        log << TestLog::Section("Description", "Test description");
        log << TestLog::Message << "Testing effects of culled fragment workload on render time" << TestLog::EndMessage;
        log << TestLog::Message
            << "Geometry consists of two fullsceen quads. The first (occluding) quad uses a trivial shader,"
               "the second (occluded) contains significant fragment shader work and a discard that is never triggers "
               "but has a dynamic condition"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "Workload indicates the number of iterations of unused work done in the occluded quad's fragment shader"
            << TestLog::EndMessage;
        log << TestLog::Message << "The ratio of rendering times of this scene with/without depth testing are compared"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "Successfull early Z-testing should result in no correlation between workload and render time"
            << TestLog::EndMessage;
        log << TestLog::EndSection;
    }
};

// Discards fragments from the occluder in a grid pattern
class PartialOccluderDiscardCullCase : public RelativeChangeCase
{
public:
    PartialOccluderDiscardCullCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name,
                                   const char *desc, int gridsize)
        : RelativeChangeCase(testCtx, renderCtx, name, desc)
        , m_gridsize(gridsize)
    {
    }
    virtual ~PartialOccluderDiscardCullCase(void)
    {
    }

private:
    virtual ObjectData genOccluderGeometry(void) const
    {
        return Utils::quadWith(Utils::getGridDiscardShader(m_gridsize), 0.2f);
    }
    virtual ObjectData genOccludedGeometry(void) const
    {
        return Utils::slowQuad(0.8f);
    }

    virtual void logDescription(void)
    {
        TestLog &log = m_testCtx.getLog();

        log << TestLog::Section("Description", "Test description");
        log << TestLog::Message << "Testing effects of partially discarded occluder on rendering time"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "Geometry consists of two fullsceen quads. The first (occluding) quad discards half the "
               "fragments in a grid pattern, the second (partially occluded) contains significant fragment shader work"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "Workload indicates the number of iterations of unused work done in the occluded quad's fragment shader"
            << TestLog::EndMessage;
        log << TestLog::Message << "The ratio of rendering times of this scene with/without depth testing are compared"
            << TestLog::EndMessage;
        log << TestLog::Message << "Successfull early Z-testing should result in depth testing halving the render time"
            << TestLog::EndMessage;
        log << TestLog::EndSection;
    }

    const int m_gridsize;
};

// Trivial occluder covering part of screen
class PartialOccluderCullCase : public RelativeChangeCase
{
public:
    PartialOccluderCullCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name, const char *desc,
                            float coverage)
        : RelativeChangeCase(testCtx, renderCtx, name, desc)
        , m_coverage(coverage)
    {
    }
    ~PartialOccluderCullCase(void)
    {
    }

private:
    virtual ObjectData genOccluderGeometry(void) const
    {
        return ObjectData(Utils::getBaseShader(), Utils::getPartScreenQuad(m_coverage, 0.2f));
    }
    virtual ObjectData genOccludedGeometry(void) const
    {
        return Utils::slowQuad(0.8f);
    }

    virtual void logDescription(void)
    {
        TestLog &log = m_testCtx.getLog();

        log << TestLog::Section("Description", "Test description");
        log << TestLog::Message << "Testing effects of partial occluder on rendering time" << TestLog::EndMessage;
        log << TestLog::Message << "Geometry consists of two quads. The first (occluding) quad covers "
            << m_coverage * 100.0f
            << "% of the screen, while the second (partially occluded, fullscreen) contains significant fragment "
               "shader work"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "Workload indicates the number of iterations of unused work done in the occluded quad's fragment shader"
            << TestLog::EndMessage;
        log << TestLog::Message << "The ratio of rendering times of this scene with/without depth testing are compared"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "Successfull early Z-testing should result in render time increasing proportionally with unoccluded area"
            << TestLog::EndMessage;
        log << TestLog::EndSection;
    }

    const float m_coverage;
};

// Constant offset to frag depth in occluder
class StaticOccluderFragDepthCullCase : public RelativeChangeCase
{
public:
    StaticOccluderFragDepthCullCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name,
                                    const char *desc)
        : RelativeChangeCase(testCtx, renderCtx, name, desc)
    {
    }

    ~StaticOccluderFragDepthCullCase(void)
    {
    }

private:
    virtual ObjectData genOccluderGeometry(void) const
    {
        return Utils::quadWith(Utils::getStaticFragDepthFragmentShader(), 0.2f);
    }
    virtual ObjectData genOccludedGeometry(void) const
    {
        return Utils::slowQuad(0.8f);
    }

    virtual void logDescription(void)
    {
        TestLog &log = m_testCtx.getLog();

        log << TestLog::Section("Description", "Test description");
        log << TestLog::Message << "Testing effects of non-default frag depth on culling efficiency"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "Geometry consists of two fullscreen quads. The first (occluding) quad is trivial, while the second "
               "(occluded) contains significant fragment shader work"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "Workload indicates the number of iterations of unused work done in the occluded quad's fragment shader"
            << TestLog::EndMessage;
        log << TestLog::Message << "The occluder quad has a static offset applied to gl_FragDepth"
            << TestLog::EndMessage;
        log << TestLog::Message << "The ratio of rendering times of this scene with/without depth testing are compared"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "Successfull early Z-testing should result in no correlation between workload and render time"
            << TestLog::EndMessage;
        log << TestLog::EndSection;
    }
};

// Dynamic offset to frag depth in occluder
class DynamicOccluderFragDepthCullCase : public RelativeChangeCase
{
public:
    DynamicOccluderFragDepthCullCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name,
                                     const char *desc)
        : RelativeChangeCase(testCtx, renderCtx, name, desc)
    {
    }

    ~DynamicOccluderFragDepthCullCase(void)
    {
    }

private:
    virtual ObjectData genOccluderGeometry(void) const
    {
        return Utils::quadWith(Utils::getDynamicFragDepthFragmentShader(), 0.2f);
    }
    virtual ObjectData genOccludedGeometry(void) const
    {
        return Utils::slowQuad(0.8f);
    }

    virtual void logDescription(void)
    {
        TestLog &log = m_testCtx.getLog();

        log << TestLog::Section("Description", "Test description");
        log << TestLog::Message << "Testing effects of non-default frag depth on culling efficiency"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "Geometry consists of two fullscreen quads. The first (occluding) quad is trivial, while the second "
               "(occluded) contains significant fragment shader work"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "Workload indicates the number of iterations of unused work done in the occluded quad's fragment shader"
            << TestLog::EndMessage;
        log << TestLog::Message << "The occluder quad has a dynamic offset applied to gl_FragDepth"
            << TestLog::EndMessage;
        log << TestLog::Message << "The ratio of rendering times of this scene with/without depth testing are compared"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "Successfull early Z-testing should result in no correlation between workload and render time"
            << TestLog::EndMessage;
        log << TestLog::EndSection;
    }
};

// Constant offset to frag depth in occluded
class StaticOccludedFragDepthCullCase : public RelativeChangeCase
{
public:
    StaticOccludedFragDepthCullCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name,
                                    const char *desc)
        : RelativeChangeCase(testCtx, renderCtx, name, desc)
    {
    }

    ~StaticOccludedFragDepthCullCase(void)
    {
    }

private:
    virtual ObjectData genOccluderGeometry(void) const
    {
        return Utils::fastQuad(0.2f);
    }
    virtual ObjectData genOccludedGeometry(void) const
    {
        return Utils::quadWith(Utils::getStaticFragDepthArithmeticWorkloadFragmentShader(), 0.2f);
    }

    virtual void logDescription(void)
    {
        TestLog &log = m_testCtx.getLog();

        log << TestLog::Section("Description", "Test description");
        log << TestLog::Message << "Testing effects of non-default frag depth on rendering time" << TestLog::EndMessage;
        log << TestLog::Message
            << "Geometry consists of two fullscreen quads. The first (occluding) quad is trivial, while the second "
               "(occluded) contains significant fragment shader work"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "Workload indicates the number of iterations of unused work done in the occluded quad's fragment shader"
            << TestLog::EndMessage;
        log << TestLog::Message << "The occluded quad has a static offset applied to gl_FragDepth"
            << TestLog::EndMessage;
        log << TestLog::Message << "The ratio of rendering times of this scene with/without depth testing are compared"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "Successfull early Z-testing should result in no correlation between workload and render time"
            << TestLog::EndMessage;
        log << TestLog::EndSection;
    }
};

// Dynamic offset to frag depth in occluded
class DynamicOccludedFragDepthCullCase : public RelativeChangeCase
{
public:
    DynamicOccludedFragDepthCullCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name,
                                     const char *desc)
        : RelativeChangeCase(testCtx, renderCtx, name, desc)
    {
    }

    ~DynamicOccludedFragDepthCullCase(void)
    {
    }

private:
    virtual ObjectData genOccluderGeometry(void) const
    {
        return Utils::fastQuad(0.2f);
    }
    virtual ObjectData genOccludedGeometry(void) const
    {
        return Utils::quadWith(Utils::getDynamicFragDepthArithmeticWorkloadFragmentShader(), 0.2f);
    }

    virtual void logDescription(void)
    {
        TestLog &log = m_testCtx.getLog();

        log << TestLog::Section("Description", "Test description");
        log << TestLog::Message << "Testing effects of non-default frag depth on rendering time" << TestLog::EndMessage;
        log << TestLog::Message
            << "Geometry consists of two fullscreen quads. The first (occluding) quad is trivial, while the second "
               "(occluded) contains significant fragment shader work"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "Workload indicates the number of iterations of unused work done in the occluded quad's fragment shader"
            << TestLog::EndMessage;
        log << TestLog::Message << "The occluded quad has a dynamic offset applied to gl_FragDepth"
            << TestLog::EndMessage;
        log << TestLog::Message << "The ratio of rendering times of this scene with/without depth testing are compared"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "Successfull early Z-testing should result in no correlation between workload and render time"
            << TestLog::EndMessage;
        log << TestLog::EndSection;
    }
};

// Dynamic offset to frag depth in occluded
class ReversedDepthOrderCullCase : public RelativeChangeCase
{
public:
    ReversedDepthOrderCullCase(TestContext &testCtx, const RenderContext &renderCtx, const char *name, const char *desc)
        : RelativeChangeCase(testCtx, renderCtx, name, desc)
    {
    }

    ~ReversedDepthOrderCullCase(void)
    {
    }

private:
    virtual ObjectData genOccluderGeometry(void) const
    {
        return Utils::fastQuad(0.2f);
    }
    virtual ObjectData genOccludedGeometry(void) const
    {
        return Utils::slowQuad(0.8f);
    }

    virtual void logDescription(void)
    {
        TestLog &log = m_testCtx.getLog();

        log << TestLog::Section("Description", "Test description");
        log << TestLog::Message << "Testing effects of of back first rendering order on culling efficiency"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "Geometry consists of two fullscreen quads. The second (occluding) quad is trivial, while the first "
               "(occluded) contains significant fragment shader work"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "Workload indicates the number of iterations of unused work done in the occluded quad's fragment shader"
            << TestLog::EndMessage;
        log << TestLog::Message << "The ratio of rendering times of this scene with/without depth testing are compared"
            << TestLog::EndMessage;
        log << TestLog::Message
            << "Successfull early Z-testing should result in no correlation between workload and render time"
            << TestLog::EndMessage;
        log << TestLog::EndSection;
    }

    // Rendering order of occluder & occluded is reversed, otherwise identical to parent version
    Sample renderSample(const RenderData &occluder, const RenderData &occluded, int workload) const
    {
        const glw::Functions &gl = m_renderCtx.getFunctions();
        const GLuint program     = occluded.m_program.getProgram();
        Sample sample;
        uint64_t now  = 0;
        uint64_t prev = 0;
        uint8_t buffer[4];

        gl.useProgram(program);
        gl.uniform1i(gl.getUniformLocation(program, "u_iterations"), workload);

        // Warmup (this workload seems to reduce variation in following workloads)
        {
            gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            gl.disable(GL_DEPTH_TEST);

            gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
        }

        // Null time
        {
            prev = deGetMicroseconds();

            gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            gl.disable(GL_DEPTH_TEST);

            gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

            now = deGetMicroseconds();

            sample.nullTime = now - prev;
        }

        // Test time
        {
            prev = deGetMicroseconds();

            gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            gl.enable(GL_DEPTH_TEST);

            render(occluded);
            render(occluder);

            gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

            now = deGetMicroseconds();

            sample.testTime = now - prev;
        }

        // Base time
        {
            prev = deGetMicroseconds();

            gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            gl.disable(GL_DEPTH_TEST);

            render(occluded);
            render(occluder);

            gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

            now = deGetMicroseconds();

            sample.baseTime = now - prev;
        }

        sample.workload = 0;

        return sample;
    }
};

} // namespace

DepthTests::DepthTests(Context &context) : TestCaseGroup(context, "depth", "Depth culling performance")
{
}

void DepthTests::init(void)
{
    TestContext &testCtx           = m_context.getTestContext();
    const RenderContext &renderCtx = m_context.getRenderContext();

    {
        tcu::TestCaseGroup *const cullEfficiencyGroup =
            new tcu::TestCaseGroup(m_testCtx, "cull_efficiency", "Fragment cull efficiency");

        addChild(cullEfficiencyGroup);

        {
            tcu::TestCaseGroup *const group = new tcu::TestCaseGroup(m_testCtx, "workload", "Workload");

            cullEfficiencyGroup->addChild(group);

            group->addChild(new FragmentTextureWorkloadCullCase(testCtx, renderCtx, "workload_texture",
                                                                "Fragment shader with texture lookup workload"));
            group->addChild(new FragmentArithmeticWorkloadCullCase(testCtx, renderCtx, "workload_arithmetic",
                                                                   "Fragment shader with arithmetic workload"));
            group->addChild(new FragmentDiscardArithmeticWorkloadCullCase(
                testCtx, renderCtx, "workload_arithmetic_discard",
                "Fragment shader that may discard with arithmetic workload"));
        }

        {
            tcu::TestCaseGroup *const group = new tcu::TestCaseGroup(m_testCtx, "occluder_discard", "Discard");

            cullEfficiencyGroup->addChild(group);

            group->addChild(new PartialOccluderDiscardCullCase(testCtx, renderCtx, "grid_256",
                                                               "Parts of occluder geometry discarded", 256));
            group->addChild(new PartialOccluderDiscardCullCase(testCtx, renderCtx, "grid_128",
                                                               "Parts of occluder geometry discarded", 128));
            group->addChild(new PartialOccluderDiscardCullCase(testCtx, renderCtx, "grid_64",
                                                               "Parts of occluder geometry discarded", 64));
            group->addChild(new PartialOccluderDiscardCullCase(testCtx, renderCtx, "grid_32",
                                                               "Parts of occluder geometry discarded", 32));
            group->addChild(new PartialOccluderDiscardCullCase(testCtx, renderCtx, "grid_16",
                                                               "Parts of occluder geometry discarded", 16));
            group->addChild(new PartialOccluderDiscardCullCase(testCtx, renderCtx, "grid_8",
                                                               "Parts of occluder geometry discarded", 8));
            group->addChild(new PartialOccluderDiscardCullCase(testCtx, renderCtx, "grid_4",
                                                               "Parts of occluder geometry discarded", 4));
            group->addChild(new PartialOccluderDiscardCullCase(testCtx, renderCtx, "grid_2",
                                                               "Parts of occluder geometry discarded", 2));
            group->addChild(new PartialOccluderDiscardCullCase(testCtx, renderCtx, "grid_1",
                                                               "Parts of occluder geometry discarded", 1));
        }

        {
            tcu::TestCaseGroup *const group = new tcu::TestCaseGroup(m_testCtx, "partial_coverage", "Partial Coverage");

            cullEfficiencyGroup->addChild(group);

            group->addChild(new PartialOccluderCullCase(testCtx, renderCtx, "100",
                                                        "Occluder covering only part of occluded geometry", 1.00f));
            group->addChild(new PartialOccluderCullCase(testCtx, renderCtx, "099",
                                                        "Occluder covering only part of occluded geometry", 0.99f));
            group->addChild(new PartialOccluderCullCase(testCtx, renderCtx, "095",
                                                        "Occluder covering only part of occluded geometry", 0.95f));
            group->addChild(new PartialOccluderCullCase(testCtx, renderCtx, "090",
                                                        "Occluder covering only part of occluded geometry", 0.90f));
            group->addChild(new PartialOccluderCullCase(testCtx, renderCtx, "080",
                                                        "Occluder covering only part of occluded geometry", 0.80f));
            group->addChild(new PartialOccluderCullCase(testCtx, renderCtx, "070",
                                                        "Occluder covering only part of occluded geometry", 0.70f));
            group->addChild(new PartialOccluderCullCase(testCtx, renderCtx, "050",
                                                        "Occluder covering only part of occluded geometry", 0.50f));
            group->addChild(new PartialOccluderCullCase(testCtx, renderCtx, "025",
                                                        "Occluder covering only part of occluded geometry", 0.25f));
            group->addChild(new PartialOccluderCullCase(testCtx, renderCtx, "010",
                                                        "Occluder covering only part of occluded geometry", 0.10f));
        }

        {
            tcu::TestCaseGroup *const group = new tcu::TestCaseGroup(m_testCtx, "frag_depth", "Partial Coverage");

            cullEfficiencyGroup->addChild(group);

            group->addChild(new StaticOccluderFragDepthCullCase(testCtx, renderCtx, "occluder_static", ""));
            group->addChild(new DynamicOccluderFragDepthCullCase(testCtx, renderCtx, "occluder_dynamic", ""));
            group->addChild(new StaticOccludedFragDepthCullCase(testCtx, renderCtx, "occluded_static", ""));
            group->addChild(new DynamicOccludedFragDepthCullCase(testCtx, renderCtx, "occluded_dynamic", ""));
        }

        {
            tcu::TestCaseGroup *const group = new tcu::TestCaseGroup(m_testCtx, "order", "Rendering order");

            cullEfficiencyGroup->addChild(group);

            group->addChild(
                new ReversedDepthOrderCullCase(testCtx, renderCtx, "reversed", "Back to front rendering order"));
        }
    }

    {
        tcu::TestCaseGroup *const testCostGroup =
            new tcu::TestCaseGroup(m_testCtx, "culled_pixel_cost", "Fragment cull efficiency");

        addChild(testCostGroup);

        {
            tcu::TestCaseGroup *const group =
                new tcu::TestCaseGroup(m_testCtx, "gradient", "Gradients with small depth differences");

            testCostGroup->addChild(group);

            group->addChild(new BaseCostCase(testCtx, renderCtx, "flat", ""));
            group->addChild(new GradientCostCase(testCtx, renderCtx, "gradient_050", "", 0.50f));
            group->addChild(new GradientCostCase(testCtx, renderCtx, "gradient_010", "", 0.10f));
            group->addChild(new GradientCostCase(testCtx, renderCtx, "gradient_005", "", 0.05f));
            group->addChild(new GradientCostCase(testCtx, renderCtx, "gradient_002", "", 0.02f));
            group->addChild(new GradientCostCase(testCtx, renderCtx, "gradient_001", "", 0.01f));
        }

        {
            tcu::TestCaseGroup *const group =
                new tcu::TestCaseGroup(m_testCtx, "occluder_geometry", "Occluders with varying geometry complexity");

            testCostGroup->addChild(group);

            group->addChild(
                new OccludingGeometryComplexityCostCase(testCtx, renderCtx, "flat_uniform_grid_5", "", 5, 0.0f, 0.0f));
            group->addChild(new OccludingGeometryComplexityCostCase(testCtx, renderCtx, "flat_uniform_grid_15", "", 15,
                                                                    0.0f, 0.0f));
            group->addChild(new OccludingGeometryComplexityCostCase(testCtx, renderCtx, "flat_uniform_grid_25", "", 25,
                                                                    0.0f, 0.0f));
            group->addChild(new OccludingGeometryComplexityCostCase(testCtx, renderCtx, "flat_uniform_grid_50", "", 50,
                                                                    0.0f, 0.0f));
            group->addChild(new OccludingGeometryComplexityCostCase(testCtx, renderCtx, "flat_uniform_grid_100", "",
                                                                    100, 0.0f, 0.0f));

            group->addChild(new OccludingGeometryComplexityCostCase(testCtx, renderCtx, "flat_noisy_grid_5", "", 5,
                                                                    1.0f / 5.0f, 0.0f));
            group->addChild(new OccludingGeometryComplexityCostCase(testCtx, renderCtx, "flat_noisy_grid_15", "", 15,
                                                                    1.0f / 15.0f, 0.0f));
            group->addChild(new OccludingGeometryComplexityCostCase(testCtx, renderCtx, "flat_noisy_grid_25", "", 25,
                                                                    1.0f / 25.0f, 0.0f));
            group->addChild(new OccludingGeometryComplexityCostCase(testCtx, renderCtx, "flat_noisy_grid_50", "", 50,
                                                                    1.0f / 50.0f, 0.0f));
            group->addChild(new OccludingGeometryComplexityCostCase(testCtx, renderCtx, "flat_noisy_grid_100", "", 100,
                                                                    1.0f / 100.0f, 0.0f));

            group->addChild(new OccludingGeometryComplexityCostCase(testCtx, renderCtx, "uneven_uniform_grid_5", "", 5,
                                                                    0.0f, 0.2f));
            group->addChild(new OccludingGeometryComplexityCostCase(testCtx, renderCtx, "uneven_uniform_grid_15", "",
                                                                    15, 0.0f, 0.2f));
            group->addChild(new OccludingGeometryComplexityCostCase(testCtx, renderCtx, "uneven_uniform_grid_25", "",
                                                                    25, 0.0f, 0.2f));
            group->addChild(new OccludingGeometryComplexityCostCase(testCtx, renderCtx, "uneven_uniform_grid_50", "",
                                                                    50, 0.0f, 0.2f));
            group->addChild(new OccludingGeometryComplexityCostCase(testCtx, renderCtx, "uneven_uniform_grid_100", "",
                                                                    100, 0.0f, 0.2f));

            group->addChild(new OccludingGeometryComplexityCostCase(testCtx, renderCtx, "uneven_noisy_grid_5", "", 5,
                                                                    1.0f / 5.0f, 0.2f));
            group->addChild(new OccludingGeometryComplexityCostCase(testCtx, renderCtx, "uneven_noisy_grid_15", "", 15,
                                                                    1.0f / 15.0f, 0.2f));
            group->addChild(new OccludingGeometryComplexityCostCase(testCtx, renderCtx, "uneven_noisy_grid_25", "", 25,
                                                                    1.0f / 25.0f, 0.2f));
            group->addChild(new OccludingGeometryComplexityCostCase(testCtx, renderCtx, "uneven_noisy_grid_50", "", 50,
                                                                    1.0f / 50.0f, 0.2f));
            group->addChild(new OccludingGeometryComplexityCostCase(testCtx, renderCtx, "uneven_noisy_grid_100", "",
                                                                    100, 1.0f / 100.0f, 0.2f));
        }

        {
            tcu::TestCaseGroup *const group = new tcu::TestCaseGroup(m_testCtx, "frag_depth", "Modifying gl_FragDepth");

            testCostGroup->addChild(group);

            group->addChild(new OccluderStaticFragDepthCostCase(testCtx, renderCtx, "occluder_static", ""));
            group->addChild(new OccluderDynamicFragDepthCostCase(testCtx, renderCtx, "occluder_dynamic", ""));
            group->addChild(new OccludedStaticFragDepthCostCase(testCtx, renderCtx, "occluded_static", ""));
            group->addChild(new OccludedDynamicFragDepthCostCase(testCtx, renderCtx, "occluded_dynamic", ""));
        }
    }
}

} // namespace Performance
} // namespace gles3
} // namespace deqp
