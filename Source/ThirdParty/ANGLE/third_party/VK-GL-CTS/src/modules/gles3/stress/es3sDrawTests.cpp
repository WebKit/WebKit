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
 * \brief Draw stress tests
 *//*--------------------------------------------------------------------*/

#include "es3sDrawTests.hpp"
#include "tcuVector.hpp"
#include "tcuTestLog.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuSurface.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluObjectWrapper.hpp"
#include "gluPixelTransfer.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"
#include "gluStrUtil.hpp"
#include "glsDrawTest.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <set>

namespace deqp
{
namespace gles3
{
namespace Stress
{
namespace
{

static const char *const s_vertexSource   = "#version 300 es\n"
                                            "in highp vec4 a_position;\n"
                                            "void main (void)\n"
                                            "{\n"
                                            "    gl_Position = a_position;\n"
                                            "}\n";
static const char *const s_fragmentSource = "#version 300 es\n"
                                            "layout(location = 0) out mediump vec4 fragColor;\n"
                                            "void main (void)\n"
                                            "{\n"
                                            "    fragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
                                            "}\n";

class DrawInvalidRangeCase : public TestCase
{
public:
    DrawInvalidRangeCase(Context &ctx, const char *name, const char *desc, uint32_t min, uint32_t max,
                         bool useLimitMin = false, bool useLimitMax = false);
    ~DrawInvalidRangeCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    const int m_min;
    const int m_max;
    const int m_bufferedElements;
    const int m_numIndices;
    const bool m_useLimitMin;
    const bool m_useLimitMax;

    uint32_t m_buffer;
    uint32_t m_indexBuffer;
    glu::ShaderProgram *m_program;
};

DrawInvalidRangeCase::DrawInvalidRangeCase(Context &ctx, const char *name, const char *desc, uint32_t min, uint32_t max,
                                           bool useLimitMin, bool useLimitMax)
    : TestCase(ctx, name, desc)
    , m_min(min)
    , m_max(max)
    , m_bufferedElements(128)
    , m_numIndices(64)
    , m_useLimitMin(useLimitMin)
    , m_useLimitMax(useLimitMax)
    , m_buffer(0)
    , m_indexBuffer(0)
    , m_program(nullptr)
{
}

DrawInvalidRangeCase::~DrawInvalidRangeCase(void)
{
    deinit();
}

void DrawInvalidRangeCase::init(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    std::vector<tcu::Vec4> data(m_bufferedElements); // !< some junk data to make sure buffer is really allocated
    std::vector<uint32_t> indices(m_numIndices);

    for (int ndx = 0; ndx < m_numIndices; ++ndx)
        indices[ndx] = ndx;

    gl.genBuffers(1, &m_buffer);
    gl.bindBuffer(GL_ARRAY_BUFFER, m_buffer);
    gl.bufferData(GL_ARRAY_BUFFER, int(m_bufferedElements * sizeof(tcu::Vec4)), &data[0], GL_STATIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "buffer gen");

    gl.genBuffers(1, &m_indexBuffer);
    gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
    gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, int(m_numIndices * sizeof(uint32_t)), &indices[0], GL_STATIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "buffer gen");

    m_program = new glu::ShaderProgram(m_context.getRenderContext(), glu::ProgramSources()
                                                                         << glu::VertexSource(s_vertexSource)
                                                                         << glu::FragmentSource(s_fragmentSource));
    if (!m_program->isOk())
    {
        m_testCtx.getLog() << *m_program;
        throw tcu::TestError("could not build program");
    }
}

void DrawInvalidRangeCase::deinit(void)
{
    if (m_buffer)
    {
        m_context.getRenderContext().getFunctions().deleteBuffers(1, &m_buffer);
        m_buffer = 0;
    }

    if (m_indexBuffer)
    {
        m_context.getRenderContext().getFunctions().deleteBuffers(1, &m_indexBuffer);
        m_indexBuffer = 0;
    }

    delete m_program;
    m_program = nullptr;
}

DrawInvalidRangeCase::IterateResult DrawInvalidRangeCase::iterate(void)
{
    glu::CallLogWrapper gl(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
    const int32_t positionLoc = gl.glGetAttribLocation(m_program->getProgram(), "a_position");
    tcu::Surface dst(m_context.getRenderTarget().getWidth(), m_context.getRenderTarget().getHeight());
    glu::VertexArray vao(m_context.getRenderContext());

    int64_t indexLimit = 0;
    uint32_t min       = m_min;
    uint32_t max       = m_max;

    gl.enableLogging(true);

    if (m_useLimitMin || m_useLimitMax)
    {
        gl.glGetInteger64v(GL_MAX_ELEMENT_INDEX, &indexLimit);
        GLU_EXPECT_NO_ERROR(gl.glGetError(), "query limit");
    }

    if (m_useLimitMin)
    {
        if ((uint64_t)indexLimit > 0xFFFFFFFFULL)
            min = 0xFFFFFFF0;
        else
            min = (uint32_t)(indexLimit - 16);
    }

    if (m_useLimitMax)
    {
        if ((uint64_t)indexLimit > 0xFFFFFFFFULL)
            max = 0xFFFFFFFF;
        else
            max = (uint32_t)indexLimit;
    }

    gl.glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gl.glClear(GL_COLOR_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.glGetError(), "setup");

    gl.glUseProgram(m_program->getProgram());
    GLU_EXPECT_NO_ERROR(gl.glGetError(), "use program");

    gl.glBindVertexArray(*vao);
    gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);

    gl.glBindBuffer(GL_ARRAY_BUFFER, m_buffer);
    gl.glEnableVertexAttribArray(positionLoc);
    gl.glVertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    GLU_EXPECT_NO_ERROR(gl.glGetError(), "set buffer");

    gl.glDrawRangeElements(GL_POINTS, min, max, m_numIndices, GL_UNSIGNED_INT, nullptr);

    // Indexing outside range is an error, but it doesnt need to be checked. Causes implementation-dependent behavior.
    // Even if the indices are in range (m_min = 0), the specification allows partial processing of vertices in the range,
    // which might cause access over buffer bounds. Causes implementation-dependent behavior.

    // allow errors
    {
        const uint32_t error = gl.glGetError();

        if (error != GL_NO_ERROR)
            m_testCtx.getLog() << tcu::TestLog::Message << "Got error: " << glu::getErrorStr(error) << ", ignoring..."
                               << tcu::TestLog::EndMessage;
    }

    // read pixels to wait for rendering
    gl.glFinish();
    glu::readPixels(m_context.getRenderContext(), 0, 0, dst.getAccess());

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    return STOP;
}

static void genBasicSpec(gls::DrawTestSpec &spec, gls::DrawTestSpec::DrawMethod method)
{
    spec.apiType            = glu::ApiType::es(3, 0);
    spec.primitive          = gls::DrawTestSpec::PRIMITIVE_TRIANGLES;
    spec.primitiveCount     = 5;
    spec.drawMethod         = method;
    spec.indexType          = gls::DrawTestSpec::INDEXTYPE_LAST;
    spec.indexPointerOffset = 0;
    spec.indexStorage       = gls::DrawTestSpec::STORAGE_LAST;
    spec.first              = 0;
    spec.indexMin           = 0;
    spec.indexMax           = 0;
    spec.instanceCount      = 1;

    spec.attribs.resize(2);

    spec.attribs[0].inputType           = gls::DrawTestSpec::INPUTTYPE_FLOAT;
    spec.attribs[0].outputType          = gls::DrawTestSpec::OUTPUTTYPE_VEC2;
    spec.attribs[0].storage             = gls::DrawTestSpec::STORAGE_BUFFER;
    spec.attribs[0].usage               = gls::DrawTestSpec::USAGE_STATIC_DRAW;
    spec.attribs[0].componentCount      = 4;
    spec.attribs[0].offset              = 0;
    spec.attribs[0].stride              = 0;
    spec.attribs[0].normalize           = false;
    spec.attribs[0].instanceDivisor     = 0;
    spec.attribs[0].useDefaultAttribute = false;

    spec.attribs[1].inputType           = gls::DrawTestSpec::INPUTTYPE_FLOAT;
    spec.attribs[1].outputType          = gls::DrawTestSpec::OUTPUTTYPE_VEC2;
    spec.attribs[1].storage             = gls::DrawTestSpec::STORAGE_BUFFER;
    spec.attribs[1].usage               = gls::DrawTestSpec::USAGE_STATIC_DRAW;
    spec.attribs[1].componentCount      = 2;
    spec.attribs[1].offset              = 0;
    spec.attribs[1].stride              = 0;
    spec.attribs[1].normalize           = false;
    spec.attribs[1].instanceDivisor     = 0;
    spec.attribs[1].useDefaultAttribute = false;
}

class IndexGroup : public TestCaseGroup
{
public:
    IndexGroup(Context &context, const char *name, const char *descr, gls::DrawTestSpec::DrawMethod drawMethod);
    ~IndexGroup(void);

    void init(void);

private:
    gls::DrawTestSpec::DrawMethod m_method;
};

IndexGroup::IndexGroup(Context &context, const char *name, const char *descr, gls::DrawTestSpec::DrawMethod drawMethod)
    : TestCaseGroup(context, name, descr)
    , m_method(drawMethod)
{
}

IndexGroup::~IndexGroup(void)
{
}

void IndexGroup::init(void)
{
    struct IndexTest
    {
        gls::DrawTestSpec::Storage storage;
        gls::DrawTestSpec::IndexType type;
        bool aligned;
        int offsets[3];
    };

    const IndexTest tests[] = {
        {gls::DrawTestSpec::STORAGE_BUFFER, gls::DrawTestSpec::INDEXTYPE_SHORT, false, {1, 3, -1}},
        {gls::DrawTestSpec::STORAGE_BUFFER, gls::DrawTestSpec::INDEXTYPE_INT, false, {2, 3, -1}},
    };

    gls::DrawTestSpec spec;

    tcu::TestCaseGroup *const unalignedBufferGroup =
        new tcu::TestCaseGroup(m_testCtx, "unaligned_buffer", "unaligned buffer");
    const bool isRangedMethod = (m_method == gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_RANGED ||
                                 m_method == gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_RANGED_BASEVERTEX);

    genBasicSpec(spec, m_method);

    this->addChild(unalignedBufferGroup);

    for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(tests); ++testNdx)
    {
        const IndexTest &indexTest = tests[testNdx];

        DE_ASSERT(indexTest.storage != gls::DrawTestSpec::STORAGE_USER);
        DE_ASSERT(!indexTest.aligned);
        tcu::TestCaseGroup *group = unalignedBufferGroup;

        const std::string name = std::string("index_") + gls::DrawTestSpec::indexTypeToString(indexTest.type);
        const std::string desc = std::string("index ") + gls::DrawTestSpec::indexTypeToString(indexTest.type) + " in " +
                                 gls::DrawTestSpec::storageToString(indexTest.storage);
        de::MovePtr<gls::DrawTest> test(
            new gls::DrawTest(m_testCtx, m_context.getRenderContext(), name.c_str(), desc.c_str()));

        spec.indexType    = indexTest.type;
        spec.indexStorage = indexTest.storage;

        if (isRangedMethod)
        {
            spec.indexMin = 0;
            spec.indexMax = 55;
        }

        for (int iterationNdx = 0;
             iterationNdx < DE_LENGTH_OF_ARRAY(indexTest.offsets) && indexTest.offsets[iterationNdx] != -1;
             ++iterationNdx)
        {
            const std::string iterationDesc = std::string("offset ") + de::toString(indexTest.offsets[iterationNdx]);
            spec.indexPointerOffset         = indexTest.offsets[iterationNdx];
            test->addIteration(spec, iterationDesc.c_str());
        }

        DE_ASSERT(spec.isCompatibilityTest() == gls::DrawTestSpec::COMPATIBILITY_UNALIGNED_OFFSET ||
                  spec.isCompatibilityTest() == gls::DrawTestSpec::COMPATIBILITY_UNALIGNED_STRIDE);
        group->addChild(test.release());
    }
}

class MethodGroup : public TestCaseGroup
{
public:
    MethodGroup(Context &context, const char *name, const char *descr, gls::DrawTestSpec::DrawMethod drawMethod);
    ~MethodGroup(void);

    void init(void);

private:
    gls::DrawTestSpec::DrawMethod m_method;
};

MethodGroup::MethodGroup(Context &context, const char *name, const char *descr,
                         gls::DrawTestSpec::DrawMethod drawMethod)
    : TestCaseGroup(context, name, descr)
    , m_method(drawMethod)
{
}

MethodGroup::~MethodGroup(void)
{
}

void MethodGroup::init(void)
{
    const bool indexed = (m_method == gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS) ||
                         (m_method == gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_INSTANCED) ||
                         (m_method == gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_RANGED);

    DE_ASSERT(indexed);
    DE_UNREF(indexed);

    this->addChild(new IndexGroup(m_context, "indices", "Index tests", m_method));
}

class RandomGroup : public TestCaseGroup
{
public:
    RandomGroup(Context &context, const char *name, const char *descr);
    ~RandomGroup(void);

    void init(void);
};

template <int SIZE>
struct UniformWeightArray
{
    float weights[SIZE];

    UniformWeightArray(void)
    {
        for (int i = 0; i < SIZE; ++i)
            weights[i] = 1.0f;
    }
};

RandomGroup::RandomGroup(Context &context, const char *name, const char *descr) : TestCaseGroup(context, name, descr)
{
}

RandomGroup::~RandomGroup(void)
{
}

void RandomGroup::init(void)
{
    const int numAttempts = 300;

    const int attribCounts[]             = {1, 2, 5};
    const float attribWeights[]          = {30, 10, 1};
    const int primitiveCounts[]          = {1, 5, 64};
    const float primitiveCountWeights[]  = {20, 10, 1};
    const int indexOffsets[]             = {0, 7, 13};
    const float indexOffsetWeights[]     = {20, 20, 1};
    const int firsts[]                   = {0, 7, 13};
    const float firstWeights[]           = {20, 20, 1};
    const int instanceCounts[]           = {1, 2, 16, 17};
    const float instanceWeights[]        = {20, 10, 5, 1};
    const int indexMins[]                = {0, 1, 3, 8};
    const int indexMaxs[]                = {4, 8, 128, 257};
    const float indexWeights[]           = {50, 50, 50, 50};
    const int offsets[]                  = {0, 1, 5, 12};
    const float offsetWeights[]          = {50, 10, 10, 10};
    const int strides[]                  = {0, 7, 16, 17};
    const float strideWeights[]          = {50, 10, 10, 10};
    const int instanceDivisors[]         = {0, 1, 3, 129};
    const float instanceDivisorWeights[] = {70, 30, 10, 10};

    gls::DrawTestSpec::Primitive primitives[] = {
        gls::DrawTestSpec::PRIMITIVE_POINTS,       gls::DrawTestSpec::PRIMITIVE_TRIANGLES,
        gls::DrawTestSpec::PRIMITIVE_TRIANGLE_FAN, gls::DrawTestSpec::PRIMITIVE_TRIANGLE_STRIP,
        gls::DrawTestSpec::PRIMITIVE_LINES,        gls::DrawTestSpec::PRIMITIVE_LINE_STRIP,
        gls::DrawTestSpec::PRIMITIVE_LINE_LOOP};
    const UniformWeightArray<DE_LENGTH_OF_ARRAY(primitives)> primitiveWeights;

    gls::DrawTestSpec::DrawMethod drawMethods[] = {
        gls::DrawTestSpec::DRAWMETHOD_DRAWARRAYS, gls::DrawTestSpec::DRAWMETHOD_DRAWARRAYS_INSTANCED,
        gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS, gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_RANGED,
        gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_INSTANCED};
    const UniformWeightArray<DE_LENGTH_OF_ARRAY(drawMethods)> drawMethodWeights;

    gls::DrawTestSpec::IndexType indexTypes[] = {
        gls::DrawTestSpec::INDEXTYPE_BYTE,
        gls::DrawTestSpec::INDEXTYPE_SHORT,
        gls::DrawTestSpec::INDEXTYPE_INT,
    };
    const UniformWeightArray<DE_LENGTH_OF_ARRAY(indexTypes)> indexTypeWeights;

    gls::DrawTestSpec::Storage storages[] = {
        gls::DrawTestSpec::STORAGE_USER,
        gls::DrawTestSpec::STORAGE_BUFFER,
    };
    const UniformWeightArray<DE_LENGTH_OF_ARRAY(storages)> storageWeights;

    gls::DrawTestSpec::InputType inputTypes[] = {
        gls::DrawTestSpec::INPUTTYPE_FLOAT,
        gls::DrawTestSpec::INPUTTYPE_FIXED,
        gls::DrawTestSpec::INPUTTYPE_BYTE,
        gls::DrawTestSpec::INPUTTYPE_SHORT,
        gls::DrawTestSpec::INPUTTYPE_UNSIGNED_BYTE,
        gls::DrawTestSpec::INPUTTYPE_UNSIGNED_SHORT,
        gls::DrawTestSpec::INPUTTYPE_INT,
        gls::DrawTestSpec::INPUTTYPE_UNSIGNED_INT,
        gls::DrawTestSpec::INPUTTYPE_HALF,
        gls::DrawTestSpec::INPUTTYPE_UNSIGNED_INT_2_10_10_10,
        gls::DrawTestSpec::INPUTTYPE_INT_2_10_10_10,
    };
    const UniformWeightArray<DE_LENGTH_OF_ARRAY(inputTypes)> inputTypeWeights;

    gls::DrawTestSpec::OutputType outputTypes[] = {
        gls::DrawTestSpec::OUTPUTTYPE_FLOAT, gls::DrawTestSpec::OUTPUTTYPE_VEC2,  gls::DrawTestSpec::OUTPUTTYPE_VEC3,
        gls::DrawTestSpec::OUTPUTTYPE_VEC4,  gls::DrawTestSpec::OUTPUTTYPE_INT,   gls::DrawTestSpec::OUTPUTTYPE_UINT,
        gls::DrawTestSpec::OUTPUTTYPE_IVEC2, gls::DrawTestSpec::OUTPUTTYPE_IVEC3, gls::DrawTestSpec::OUTPUTTYPE_IVEC4,
        gls::DrawTestSpec::OUTPUTTYPE_UVEC2, gls::DrawTestSpec::OUTPUTTYPE_UVEC3, gls::DrawTestSpec::OUTPUTTYPE_UVEC4,
    };
    const UniformWeightArray<DE_LENGTH_OF_ARRAY(outputTypes)> outputTypeWeights;

    gls::DrawTestSpec::Usage usages[] = {
        gls::DrawTestSpec::USAGE_DYNAMIC_DRAW, gls::DrawTestSpec::USAGE_STATIC_DRAW,
        gls::DrawTestSpec::USAGE_STREAM_DRAW,  gls::DrawTestSpec::USAGE_STREAM_READ,
        gls::DrawTestSpec::USAGE_STREAM_COPY,  gls::DrawTestSpec::USAGE_STATIC_READ,
        gls::DrawTestSpec::USAGE_STATIC_COPY,  gls::DrawTestSpec::USAGE_DYNAMIC_READ,
        gls::DrawTestSpec::USAGE_DYNAMIC_COPY,
    };
    const UniformWeightArray<DE_LENGTH_OF_ARRAY(usages)> usageWeights;

    std::set<uint32_t> insertedHashes;
    size_t insertedCount = 0;

    for (int ndx = 0; ndx < numAttempts; ++ndx)
    {
        de::Random random(0xc551393 + ndx); // random does not depend on previous cases

        int attributeCount = random.chooseWeighted<int, const int *, const float *>(
            DE_ARRAY_BEGIN(attribCounts), DE_ARRAY_END(attribCounts), attribWeights);
        gls::DrawTestSpec spec;

        spec.apiType   = glu::ApiType::es(3, 0);
        spec.primitive = random.chooseWeighted<gls::DrawTestSpec::Primitive>(
            DE_ARRAY_BEGIN(primitives), DE_ARRAY_END(primitives), primitiveWeights.weights);
        spec.primitiveCount = random.chooseWeighted<int, const int *, const float *>(
            DE_ARRAY_BEGIN(primitiveCounts), DE_ARRAY_END(primitiveCounts), primitiveCountWeights);
        spec.drawMethod = random.chooseWeighted<gls::DrawTestSpec::DrawMethod>(
            DE_ARRAY_BEGIN(drawMethods), DE_ARRAY_END(drawMethods), drawMethodWeights.weights);
        spec.indexType = random.chooseWeighted<gls::DrawTestSpec::IndexType>(
            DE_ARRAY_BEGIN(indexTypes), DE_ARRAY_END(indexTypes), indexTypeWeights.weights);
        spec.indexPointerOffset = random.chooseWeighted<int, const int *, const float *>(
            DE_ARRAY_BEGIN(indexOffsets), DE_ARRAY_END(indexOffsets), indexOffsetWeights);
        spec.indexStorage = random.chooseWeighted<gls::DrawTestSpec::Storage>(
            DE_ARRAY_BEGIN(storages), DE_ARRAY_END(storages), storageWeights.weights);
        spec.first         = random.chooseWeighted<int, const int *, const float *>(DE_ARRAY_BEGIN(firsts),
                                                                            DE_ARRAY_END(firsts), firstWeights);
        spec.indexMin      = random.chooseWeighted<int, const int *, const float *>(DE_ARRAY_BEGIN(indexMins),
                                                                               DE_ARRAY_END(indexMins), indexWeights);
        spec.indexMax      = random.chooseWeighted<int, const int *, const float *>(DE_ARRAY_BEGIN(indexMaxs),
                                                                               DE_ARRAY_END(indexMaxs), indexWeights);
        spec.instanceCount = random.chooseWeighted<int, const int *, const float *>(
            DE_ARRAY_BEGIN(instanceCounts), DE_ARRAY_END(instanceCounts), instanceWeights);

        // check spec is legal
        if (!spec.valid())
            continue;

        for (int attrNdx = 0; attrNdx < attributeCount;)
        {
            bool valid;
            gls::DrawTestSpec::AttributeSpec attribSpec;

            attribSpec.inputType = random.chooseWeighted<gls::DrawTestSpec::InputType>(
                DE_ARRAY_BEGIN(inputTypes), DE_ARRAY_END(inputTypes), inputTypeWeights.weights);
            attribSpec.outputType = random.chooseWeighted<gls::DrawTestSpec::OutputType>(
                DE_ARRAY_BEGIN(outputTypes), DE_ARRAY_END(outputTypes), outputTypeWeights.weights);
            attribSpec.storage = random.chooseWeighted<gls::DrawTestSpec::Storage>(
                DE_ARRAY_BEGIN(storages), DE_ARRAY_END(storages), storageWeights.weights);
            attribSpec.usage = random.chooseWeighted<gls::DrawTestSpec::Usage>(
                DE_ARRAY_BEGIN(usages), DE_ARRAY_END(usages), usageWeights.weights);
            attribSpec.componentCount = random.getInt(1, 4);
            attribSpec.offset         = random.chooseWeighted<int, const int *, const float *>(
                DE_ARRAY_BEGIN(offsets), DE_ARRAY_END(offsets), offsetWeights);
            attribSpec.stride = random.chooseWeighted<int, const int *, const float *>(
                DE_ARRAY_BEGIN(strides), DE_ARRAY_END(strides), strideWeights);
            attribSpec.normalize       = random.getBool();
            attribSpec.instanceDivisor = random.chooseWeighted<int, const int *, const float *>(
                DE_ARRAY_BEGIN(instanceDivisors), DE_ARRAY_END(instanceDivisors), instanceDivisorWeights);
            attribSpec.useDefaultAttribute = random.getBool();

            // check spec is legal
            valid = attribSpec.valid(spec.apiType);

            // we do not want interleaved elements. (Might result in some weird floating point values)
            if (attribSpec.stride &&
                attribSpec.componentCount * gls::DrawTestSpec::inputTypeSize(attribSpec.inputType) > attribSpec.stride)
                valid = false;

            // try again if not valid
            if (valid)
            {
                spec.attribs.push_back(attribSpec);
                ++attrNdx;
            }
        }

        // Do not collapse all vertex positions to a single positions
        if (spec.primitive != gls::DrawTestSpec::PRIMITIVE_POINTS)
            spec.attribs[0].instanceDivisor = 0;

        // Is render result meaningful?
        {
            // Only one vertex
            if (spec.drawMethod == gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_RANGED &&
                spec.indexMin == spec.indexMax && spec.primitive != gls::DrawTestSpec::PRIMITIVE_POINTS)
                continue;
            if (spec.attribs[0].useDefaultAttribute && spec.primitive != gls::DrawTestSpec::PRIMITIVE_POINTS)
                continue;

            // Triangle only on one axis
            if (spec.primitive == gls::DrawTestSpec::PRIMITIVE_TRIANGLES ||
                spec.primitive == gls::DrawTestSpec::PRIMITIVE_TRIANGLE_FAN ||
                spec.primitive == gls::DrawTestSpec::PRIMITIVE_TRIANGLE_STRIP)
            {
                if (spec.attribs[0].componentCount == 1)
                    continue;
                if (spec.attribs[0].outputType == gls::DrawTestSpec::OUTPUTTYPE_FLOAT ||
                    spec.attribs[0].outputType == gls::DrawTestSpec::OUTPUTTYPE_INT ||
                    spec.attribs[0].outputType == gls::DrawTestSpec::OUTPUTTYPE_UINT)
                    continue;
                if (spec.drawMethod == gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_RANGED &&
                    (spec.indexMax - spec.indexMin) < 2)
                    continue;
            }
        }

        // Add case
        {
            uint32_t hash = spec.hash();
            for (int attrNdx = 0; attrNdx < attributeCount; ++attrNdx)
                hash = (hash << 2) ^ (uint32_t)spec.attribs[attrNdx].hash();

            if (insertedHashes.find(hash) == insertedHashes.end())
            {
                // Only unaligned cases
                if (spec.isCompatibilityTest() == gls::DrawTestSpec::COMPATIBILITY_UNALIGNED_OFFSET ||
                    spec.isCompatibilityTest() == gls::DrawTestSpec::COMPATIBILITY_UNALIGNED_STRIDE)
                    this->addChild(new gls::DrawTest(m_testCtx, m_context.getRenderContext(), spec,
                                                     de::toString(insertedCount).c_str(), spec.getDesc().c_str()));
                insertedHashes.insert(hash);

                ++insertedCount;
            }
        }
    }
}

} // namespace

DrawTests::DrawTests(Context &context) : TestCaseGroup(context, "draw", "Draw stress tests")
{
}

DrawTests::~DrawTests(void)
{
}

void DrawTests::init(void)
{
    tcu::TestCaseGroup *const unalignedGroup =
        new tcu::TestCaseGroup(m_testCtx, "unaligned_data", "Test with unaligned data");
    tcu::TestCaseGroup *const drawRangeGroup =
        new tcu::TestCaseGroup(m_testCtx, "draw_range_elements", "Test drawRangeElements");

    addChild(unalignedGroup);
    addChild(drawRangeGroup);

    // .unaligned_data
    {
        const gls::DrawTestSpec::DrawMethod basicMethods[] = {
            // gls::DrawTestSpec::DRAWMETHOD_DRAWARRAYS,
            gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS,
            // gls::DrawTestSpec::DRAWMETHOD_DRAWARRAYS_INSTANCED,
            gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_INSTANCED,
            gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_RANGED,
        };

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(basicMethods); ++ndx)
        {
            const std::string name = gls::DrawTestSpec::drawMethodToString(basicMethods[ndx]);
            const std::string desc = gls::DrawTestSpec::drawMethodToString(basicMethods[ndx]);

            unalignedGroup->addChild(new MethodGroup(m_context, name.c_str(), desc.c_str(), basicMethods[ndx]));
        }

        // Random

        unalignedGroup->addChild(new RandomGroup(m_context, "random", "random draw commands."));
    }

    // .draw_range_elements
    {
        // use a larger range than the buffer size is
        drawRangeGroup->addChild(new DrawInvalidRangeCase(m_context, "range_max_over_bounds",
                                                          "Range over buffer bounds", 0x00000000, 0x00210000));
        drawRangeGroup->addChild(new DrawInvalidRangeCase(m_context, "range_max_over_bounds_near_signed_wrap",
                                                          "Range over buffer bounds", 0x00000000, 0x7FFFFFFF));
        drawRangeGroup->addChild(new DrawInvalidRangeCase(m_context, "range_max_over_bounds_near_unsigned_wrap",
                                                          "Range over buffer bounds", 0x00000000, 0xFFFFFFFF));
        drawRangeGroup->addChild(new DrawInvalidRangeCase(m_context, "range_max_over_bounds_near_max",
                                                          "Range over buffer bounds", 0x00000000, 0x00000000, false,
                                                          true));

        drawRangeGroup->addChild(new DrawInvalidRangeCase(m_context, "range_min_max_over_bounds",
                                                          "Range over buffer bounds", 0x00200000, 0x00210000));
        drawRangeGroup->addChild(new DrawInvalidRangeCase(m_context, "range_min_max_over_bounds_near_signed_wrap",
                                                          "Range over buffer bounds", 0x7FFFFFF0, 0x7FFFFFFF));
        drawRangeGroup->addChild(new DrawInvalidRangeCase(m_context, "range_min_max_over_bounds_near_unsigned_wrap",
                                                          "Range over buffer bounds", 0xFFFFFFF0, 0xFFFFFFFF));
        drawRangeGroup->addChild(new DrawInvalidRangeCase(m_context, "range_min_max_over_bounds_near_max",
                                                          "Range over buffer bounds", 0x00000000, 0x00000000, true,
                                                          true));
    }
}

} // namespace Stress
} // namespace gles3
} // namespace deqp
