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
 * \brief Drawing tests.
 *//*--------------------------------------------------------------------*/

#include "es3fDrawTests.hpp"
#include "glsDrawTest.hpp"
#include "tcuRenderTarget.hpp"
#include "sglrGLContext.hpp"
#include "glwEnums.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deSTLUtil.hpp"

#include <set>

namespace deqp
{
namespace gles3
{
namespace Functional
{
namespace
{

enum TestIterationType
{
    TYPE_DRAW_COUNT,     // !< test with 1, 5, and 25 primitives
    TYPE_INSTANCE_COUNT, // !< test with 1, 4, and 11 instances
    TYPE_INDEX_RANGE,    // !< test with index range of [0, 23], [23, 40], and [5, 5]

    TYPE_LAST
};

static void addTestIterations(gls::DrawTest *test, const gls::DrawTestSpec &baseSpec, TestIterationType type)
{
    gls::DrawTestSpec spec(baseSpec);

    if (type == TYPE_DRAW_COUNT)
    {
        spec.primitiveCount = 1;
        test->addIteration(spec, "draw count = 1");

        spec.primitiveCount = 5;
        test->addIteration(spec, "draw count = 5");

        spec.primitiveCount = 25;
        test->addIteration(spec, "draw count = 25");
    }
    else if (type == TYPE_INSTANCE_COUNT)
    {
        spec.instanceCount = 1;
        test->addIteration(spec, "instance count = 1");

        spec.instanceCount = 4;
        test->addIteration(spec, "instance count = 4");

        spec.instanceCount = 11;
        test->addIteration(spec, "instance count = 11");
    }
    else if (type == TYPE_INDEX_RANGE)
    {
        spec.indexMin = 0;
        spec.indexMax = 23;
        test->addIteration(spec, "index range = [0, 23]");

        spec.indexMin = 23;
        spec.indexMax = 40;
        test->addIteration(spec, "index range = [23, 40]");

        // Only makes sense with points
        if (spec.primitive == gls::DrawTestSpec::PRIMITIVE_POINTS)
        {
            spec.indexMin = 5;
            spec.indexMax = 5;
            test->addIteration(spec, "index range = [5, 5]");
        }
    }
    else
        DE_ASSERT(false);
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

class AttributeGroup : public TestCaseGroup
{
public:
    AttributeGroup(Context &context, const char *name, const char *descr, gls::DrawTestSpec::DrawMethod drawMethod,
                   gls::DrawTestSpec::Primitive primitive, gls::DrawTestSpec::IndexType indexType,
                   gls::DrawTestSpec::Storage indexStorage);
    ~AttributeGroup(void);

    void init(void);

private:
    gls::DrawTestSpec::DrawMethod m_method;
    gls::DrawTestSpec::Primitive m_primitive;
    gls::DrawTestSpec::IndexType m_indexType;
    gls::DrawTestSpec::Storage m_indexStorage;
};

AttributeGroup::AttributeGroup(Context &context, const char *name, const char *descr,
                               gls::DrawTestSpec::DrawMethod drawMethod, gls::DrawTestSpec::Primitive primitive,
                               gls::DrawTestSpec::IndexType indexType, gls::DrawTestSpec::Storage indexStorage)
    : TestCaseGroup(context, name, descr)
    , m_method(drawMethod)
    , m_primitive(primitive)
    , m_indexType(indexType)
    , m_indexStorage(indexStorage)
{
}

AttributeGroup::~AttributeGroup(void)
{
}

void AttributeGroup::init(void)
{
    // select test type
    const bool instanced = (m_method == gls::DrawTestSpec::DRAWMETHOD_DRAWARRAYS_INSTANCED) ||
                           (m_method == gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_INSTANCED);
    const bool ranged = (m_method == gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_RANGED);
    const TestIterationType testType =
        (instanced) ? (TYPE_INSTANCE_COUNT) : ((ranged) ? (TYPE_INDEX_RANGE) : (TYPE_DRAW_COUNT));

    // Single attribute
    {
        gls::DrawTest *test =
            new gls::DrawTest(m_testCtx, m_context.getRenderContext(), "single_attribute", "Single attribute array.");
        gls::DrawTestSpec spec;

        spec.apiType            = glu::ApiType::es(3, 0);
        spec.primitive          = m_primitive;
        spec.primitiveCount     = 5;
        spec.drawMethod         = m_method;
        spec.indexType          = m_indexType;
        spec.indexPointerOffset = 0;
        spec.indexStorage       = m_indexStorage;
        spec.first              = 0;
        spec.indexMin           = 0;
        spec.indexMax           = 0;
        spec.instanceCount      = 1;

        spec.attribs.resize(1);

        spec.attribs[0].inputType           = gls::DrawTestSpec::INPUTTYPE_FLOAT;
        spec.attribs[0].outputType          = gls::DrawTestSpec::OUTPUTTYPE_VEC2;
        spec.attribs[0].storage             = gls::DrawTestSpec::STORAGE_BUFFER;
        spec.attribs[0].usage               = gls::DrawTestSpec::USAGE_STATIC_DRAW;
        spec.attribs[0].componentCount      = 2;
        spec.attribs[0].offset              = 0;
        spec.attribs[0].stride              = 0;
        spec.attribs[0].normalize           = false;
        spec.attribs[0].instanceDivisor     = 0;
        spec.attribs[0].useDefaultAttribute = false;

        addTestIterations(test, spec, testType);

        this->addChild(test);
    }

    // Multiple attribute
    {
        gls::DrawTest *test = new gls::DrawTest(m_testCtx, m_context.getRenderContext(), "multiple_attributes",
                                                "Multiple attribute arrays.");
        gls::DrawTestSpec spec;

        spec.apiType            = glu::ApiType::es(3, 0);
        spec.primitive          = m_primitive;
        spec.primitiveCount     = 5;
        spec.drawMethod         = m_method;
        spec.indexType          = m_indexType;
        spec.indexPointerOffset = 0;
        spec.indexStorage       = m_indexStorage;
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

        addTestIterations(test, spec, testType);

        this->addChild(test);
    }

    // Multiple attribute, second one divided
    {
        gls::DrawTest *test = new gls::DrawTest(m_testCtx, m_context.getRenderContext(), "instanced_attributes",
                                                "Instanced attribute array.");
        gls::DrawTestSpec spec;

        spec.apiType            = glu::ApiType::es(3, 0);
        spec.primitive          = m_primitive;
        spec.primitiveCount     = 5;
        spec.drawMethod         = m_method;
        spec.indexType          = m_indexType;
        spec.indexPointerOffset = 0;
        spec.indexStorage       = m_indexStorage;
        spec.first              = 0;
        spec.indexMin           = 0;
        spec.indexMax           = 0;
        spec.instanceCount      = 1;

        spec.attribs.resize(3);

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

        // Add another position component so the instances wont be drawn on each other
        spec.attribs[1].inputType                   = gls::DrawTestSpec::INPUTTYPE_FLOAT;
        spec.attribs[1].outputType                  = gls::DrawTestSpec::OUTPUTTYPE_VEC2;
        spec.attribs[1].storage                     = gls::DrawTestSpec::STORAGE_BUFFER;
        spec.attribs[1].usage                       = gls::DrawTestSpec::USAGE_STATIC_DRAW;
        spec.attribs[1].componentCount              = 2;
        spec.attribs[1].offset                      = 0;
        spec.attribs[1].stride                      = 0;
        spec.attribs[1].normalize                   = false;
        spec.attribs[1].instanceDivisor             = 1;
        spec.attribs[1].useDefaultAttribute         = false;
        spec.attribs[1].additionalPositionAttribute = true;

        // Instanced color
        spec.attribs[2].inputType           = gls::DrawTestSpec::INPUTTYPE_FLOAT;
        spec.attribs[2].outputType          = gls::DrawTestSpec::OUTPUTTYPE_VEC2;
        spec.attribs[2].storage             = gls::DrawTestSpec::STORAGE_BUFFER;
        spec.attribs[2].usage               = gls::DrawTestSpec::USAGE_STATIC_DRAW;
        spec.attribs[2].componentCount      = 3;
        spec.attribs[2].offset              = 0;
        spec.attribs[2].stride              = 0;
        spec.attribs[2].normalize           = false;
        spec.attribs[2].instanceDivisor     = 1;
        spec.attribs[2].useDefaultAttribute = false;

        addTestIterations(test, spec, testType);

        this->addChild(test);
    }

    // Multiple attribute, second one default
    {
        gls::DrawTest *test = new gls::DrawTest(m_testCtx, m_context.getRenderContext(), "default_attribute",
                                                "Attribute specified with glVertexAttrib*.");
        gls::DrawTestSpec spec;

        spec.apiType            = glu::ApiType::es(3, 0);
        spec.primitive          = m_primitive;
        spec.primitiveCount     = 5;
        spec.drawMethod         = m_method;
        spec.indexType          = m_indexType;
        spec.indexPointerOffset = 0;
        spec.indexStorage       = m_indexStorage;
        spec.first              = 0;
        spec.indexMin           = 0;
        spec.indexMax =
            20; // \note addTestIterations is not called for the spec, so we must ensure [indexMin, indexMax] is a good range
        spec.instanceCount = 1;

        spec.attribs.resize(2);

        spec.attribs[0].inputType           = gls::DrawTestSpec::INPUTTYPE_FLOAT;
        spec.attribs[0].outputType          = gls::DrawTestSpec::OUTPUTTYPE_VEC2;
        spec.attribs[0].storage             = gls::DrawTestSpec::STORAGE_BUFFER;
        spec.attribs[0].usage               = gls::DrawTestSpec::USAGE_STATIC_DRAW;
        spec.attribs[0].componentCount      = 2;
        spec.attribs[0].offset              = 0;
        spec.attribs[0].stride              = 0;
        spec.attribs[0].normalize           = false;
        spec.attribs[0].instanceDivisor     = 0;
        spec.attribs[0].useDefaultAttribute = false;

        struct IOPair
        {
            gls::DrawTestSpec::InputType input;
            gls::DrawTestSpec::OutputType output;
            int componentCount;
        } iopairs[] = {
            {gls::DrawTestSpec::INPUTTYPE_FLOAT, gls::DrawTestSpec::OUTPUTTYPE_VEC2, 4},
            {gls::DrawTestSpec::INPUTTYPE_FLOAT, gls::DrawTestSpec::OUTPUTTYPE_VEC4, 2},
            {gls::DrawTestSpec::INPUTTYPE_INT, gls::DrawTestSpec::OUTPUTTYPE_IVEC3, 4},
            {gls::DrawTestSpec::INPUTTYPE_UNSIGNED_INT, gls::DrawTestSpec::OUTPUTTYPE_UVEC2, 4},
        };

        for (int ioNdx = 0; ioNdx < DE_LENGTH_OF_ARRAY(iopairs); ++ioNdx)
        {
            const std::string desc = gls::DrawTestSpec::inputTypeToString(iopairs[ioNdx].input) +
                                     de::toString(iopairs[ioNdx].componentCount) + " to " +
                                     gls::DrawTestSpec::outputTypeToString(iopairs[ioNdx].output);

            spec.attribs[1].inputType           = iopairs[ioNdx].input;
            spec.attribs[1].outputType          = iopairs[ioNdx].output;
            spec.attribs[1].storage             = gls::DrawTestSpec::STORAGE_BUFFER;
            spec.attribs[1].usage               = gls::DrawTestSpec::USAGE_STATIC_DRAW;
            spec.attribs[1].componentCount      = iopairs[ioNdx].componentCount;
            spec.attribs[1].offset              = 0;
            spec.attribs[1].stride              = 0;
            spec.attribs[1].normalize           = false;
            spec.attribs[1].instanceDivisor     = 0;
            spec.attribs[1].useDefaultAttribute = true;

            test->addIteration(spec, desc.c_str());
        }

        this->addChild(test);
    }
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
        {gls::DrawTestSpec::STORAGE_USER, gls::DrawTestSpec::INDEXTYPE_BYTE, true, {0, 1, -1}},
        {gls::DrawTestSpec::STORAGE_USER, gls::DrawTestSpec::INDEXTYPE_SHORT, true, {0, 2, -1}},
        {gls::DrawTestSpec::STORAGE_USER, gls::DrawTestSpec::INDEXTYPE_INT, true, {0, 4, -1}},

        {gls::DrawTestSpec::STORAGE_USER, gls::DrawTestSpec::INDEXTYPE_SHORT, false, {1, 3, -1}},
        {gls::DrawTestSpec::STORAGE_USER, gls::DrawTestSpec::INDEXTYPE_INT, false, {2, 3, -1}},

        {gls::DrawTestSpec::STORAGE_BUFFER, gls::DrawTestSpec::INDEXTYPE_BYTE, true, {0, 1, -1}},
        {gls::DrawTestSpec::STORAGE_BUFFER, gls::DrawTestSpec::INDEXTYPE_SHORT, true, {0, 2, -1}},
        {gls::DrawTestSpec::STORAGE_BUFFER, gls::DrawTestSpec::INDEXTYPE_INT, true, {0, 4, -1}},
    };

    gls::DrawTestSpec spec;

    tcu::TestCaseGroup *userPtrGroup = new tcu::TestCaseGroup(m_testCtx, "user_ptr", "user pointer");
    tcu::TestCaseGroup *unalignedUserPtrGroup =
        new tcu::TestCaseGroup(m_testCtx, "unaligned_user_ptr", "unaligned user pointer");
    tcu::TestCaseGroup *bufferGroup = new tcu::TestCaseGroup(m_testCtx, "buffer", "buffer");

    genBasicSpec(spec, m_method);

    this->addChild(userPtrGroup);
    this->addChild(unalignedUserPtrGroup);
    this->addChild(bufferGroup);

    for (int testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(tests); ++testNdx)
    {
        const IndexTest &indexTest = tests[testNdx];
        tcu::TestCaseGroup *group  = (indexTest.storage == gls::DrawTestSpec::STORAGE_USER) ?
                                         ((indexTest.aligned) ? (userPtrGroup) : (unalignedUserPtrGroup)) :
                                         ((indexTest.aligned) ? (bufferGroup) : (nullptr));

        const std::string name = std::string("index_") + gls::DrawTestSpec::indexTypeToString(indexTest.type);
        const std::string desc = std::string("index ") + gls::DrawTestSpec::indexTypeToString(indexTest.type) + " in " +
                                 gls::DrawTestSpec::storageToString(indexTest.storage);
        de::MovePtr<gls::DrawTest> test(
            new gls::DrawTest(m_testCtx, m_context.getRenderContext(), name.c_str(), desc.c_str()));

        spec.indexType    = indexTest.type;
        spec.indexStorage = indexTest.storage;

        for (int iterationNdx = 0;
             iterationNdx < DE_LENGTH_OF_ARRAY(indexTest.offsets) && indexTest.offsets[iterationNdx] != -1;
             ++iterationNdx)
        {
            const std::string iterationDesc = std::string("offset ") + de::toString(indexTest.offsets[iterationNdx]);
            spec.indexPointerOffset         = indexTest.offsets[iterationNdx];
            test->addIteration(spec, iterationDesc.c_str());
        }

        DE_ASSERT(spec.isCompatibilityTest() != gls::DrawTestSpec::COMPATIBILITY_UNALIGNED_OFFSET);
        DE_ASSERT(spec.isCompatibilityTest() != gls::DrawTestSpec::COMPATIBILITY_UNALIGNED_STRIDE);
        group->addChild(test.release());
    }
}

class FirstGroup : public TestCaseGroup
{
public:
    FirstGroup(Context &context, const char *name, const char *descr, gls::DrawTestSpec::DrawMethod drawMethod);
    ~FirstGroup(void);

    void init(void);

private:
    gls::DrawTestSpec::DrawMethod m_method;
};

FirstGroup::FirstGroup(Context &context, const char *name, const char *descr, gls::DrawTestSpec::DrawMethod drawMethod)
    : TestCaseGroup(context, name, descr)
    , m_method(drawMethod)
{
}

FirstGroup::~FirstGroup(void)
{
}

void FirstGroup::init(void)
{
    const int firsts[] = {1, 3, 17};

    gls::DrawTestSpec spec;
    genBasicSpec(spec, m_method);

    for (int firstNdx = 0; firstNdx < DE_LENGTH_OF_ARRAY(firsts); ++firstNdx)
    {
        const std::string name = std::string("first_") + de::toString(firsts[firstNdx]);
        const std::string desc = std::string("first ") + de::toString(firsts[firstNdx]);
        gls::DrawTest *test    = new gls::DrawTest(m_testCtx, m_context.getRenderContext(), name.c_str(), desc.c_str());

        spec.first = firsts[firstNdx];

        addTestIterations(test, spec, TYPE_DRAW_COUNT);

        this->addChild(test);
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
    const bool hasFirst = (m_method == gls::DrawTestSpec::DRAWMETHOD_DRAWARRAYS) ||
                          (m_method == gls::DrawTestSpec::DRAWMETHOD_DRAWARRAYS_INSTANCED);

    const gls::DrawTestSpec::Primitive primitive[] = {
        gls::DrawTestSpec::PRIMITIVE_POINTS,       gls::DrawTestSpec::PRIMITIVE_TRIANGLES,
        gls::DrawTestSpec::PRIMITIVE_TRIANGLE_FAN, gls::DrawTestSpec::PRIMITIVE_TRIANGLE_STRIP,
        gls::DrawTestSpec::PRIMITIVE_LINES,        gls::DrawTestSpec::PRIMITIVE_LINE_STRIP,
        gls::DrawTestSpec::PRIMITIVE_LINE_LOOP};

    if (hasFirst)
    {
        // First-tests
        this->addChild(new FirstGroup(m_context, "first", "First tests", m_method));
    }

    if (indexed)
    {
        // Index-tests
        if (m_method != gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_RANGED)
            this->addChild(new IndexGroup(m_context, "indices", "Index tests", m_method));
    }

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(primitive); ++ndx)
    {
        const std::string name = gls::DrawTestSpec::primitiveToString(primitive[ndx]);
        const std::string desc = gls::DrawTestSpec::primitiveToString(primitive[ndx]);

        this->addChild(new AttributeGroup(m_context, name.c_str(), desc.c_str(), m_method, primitive[ndx],
                                          gls::DrawTestSpec::INDEXTYPE_SHORT, gls::DrawTestSpec::STORAGE_BUFFER));
    }
}

class GridProgram : public sglr::ShaderProgram
{
public:
    GridProgram(void);

    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const;
    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                        const rr::FragmentShadingContext &context) const;
};

GridProgram::GridProgram(void)
    : sglr::ShaderProgram(sglr::pdec::ShaderProgramDeclaration()
                          << sglr::pdec::VertexAttribute("a_position", rr::GENERICVECTYPE_FLOAT)
                          << sglr::pdec::VertexAttribute("a_offset", rr::GENERICVECTYPE_FLOAT)
                          << sglr::pdec::VertexAttribute("a_color", rr::GENERICVECTYPE_FLOAT)
                          << sglr::pdec::VertexToFragmentVarying(rr::GENERICVECTYPE_FLOAT)
                          << sglr::pdec::FragmentOutput(rr::GENERICVECTYPE_FLOAT)
                          << sglr::pdec::VertexSource("#version 300 es\n"
                                                      "in highp vec4 a_position;\n"
                                                      "in highp vec4 a_offset;\n"
                                                      "in highp vec4 a_color;\n"
                                                      "out mediump vec4 v_color;\n"
                                                      "void main(void)\n"
                                                      "{\n"
                                                      "    gl_Position = a_position + a_offset;\n"
                                                      "    v_color = a_color;\n"
                                                      "}\n")
                          << sglr::pdec::FragmentSource("#version 300 es\n"
                                                        "layout(location = 0) out mediump vec4 dEQP_FragColor;\n"
                                                        "in mediump vec4 v_color;\n"
                                                        "void main(void)\n"
                                                        "{\n"
                                                        "    dEQP_FragColor = v_color;\n"
                                                        "}\n"))
{
}

void GridProgram::shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets,
                                const int numPackets) const
{
    for (int ndx = 0; ndx < numPackets; ++ndx)
    {
        packets[ndx]->position =
            rr::readVertexAttribFloat(inputs[0], packets[ndx]->instanceNdx, packets[ndx]->vertexNdx) +
            rr::readVertexAttribFloat(inputs[1], packets[ndx]->instanceNdx, packets[ndx]->vertexNdx);
        packets[ndx]->outputs[0] =
            rr::readVertexAttribFloat(inputs[2], packets[ndx]->instanceNdx, packets[ndx]->vertexNdx);
    }
}

void GridProgram::shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                                 const rr::FragmentShadingContext &context) const
{
    for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        for (int fragNdx = 0; fragNdx < 4; ++fragNdx)
            rr::writeFragmentOutput(context, packetNdx, fragNdx, 0,
                                    rr::readTriangleVarying<float>(packets[packetNdx], context, 0, fragNdx));
}

class InstancedGridRenderTest : public TestCase
{
public:
    InstancedGridRenderTest(Context &context, const char *name, const char *desc, int gridSide, bool useIndices);
    ~InstancedGridRenderTest(void);

    IterateResult iterate(void);

private:
    void renderTo(sglr::Context &ctx, sglr::ShaderProgram &program, tcu::Surface &dst);
    bool verifyImage(const tcu::Surface &image);

    const int m_gridSide;
    const bool m_useIndices;
};

InstancedGridRenderTest::InstancedGridRenderTest(Context &context, const char *name, const char *desc, int gridSide,
                                                 bool useIndices)
    : TestCase(context, name, desc)
    , m_gridSide(gridSide)
    , m_useIndices(useIndices)
{
}

InstancedGridRenderTest::~InstancedGridRenderTest(void)
{
}

InstancedGridRenderTest::IterateResult InstancedGridRenderTest::iterate(void)
{
    const int renderTargetWidth  = de::min(1024, m_context.getRenderTarget().getWidth());
    const int renderTargetHeight = de::min(1024, m_context.getRenderTarget().getHeight());

    sglr::GLContext ctx(m_context.getRenderContext(), m_testCtx.getLog(),
                        sglr::GLCONTEXT_LOG_CALLS | sglr::GLCONTEXT_LOG_PROGRAMS,
                        tcu::IVec4(0, 0, renderTargetWidth, renderTargetHeight));
    tcu::Surface surface(renderTargetWidth, renderTargetHeight);
    GridProgram program;

    // render

    renderTo(ctx, program, surface);

    // verify image

    if (verifyImage(surface))
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    else
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Incorrect rendering result");
    return STOP;
}

void InstancedGridRenderTest::renderTo(sglr::Context &ctx, sglr::ShaderProgram &program, tcu::Surface &dst)
{
    const tcu::Vec4 green(0, 1, 0, 1);
    const tcu::Vec4 yellow(1, 1, 0, 1);

    uint32_t positionBuf   = 0;
    uint32_t offsetBuf     = 0;
    uint32_t colorBuf      = 0;
    uint32_t indexBuf      = 0;
    uint32_t programID     = ctx.createProgram(&program);
    int32_t posLocation    = ctx.getAttribLocation(programID, "a_position");
    int32_t offsetLocation = ctx.getAttribLocation(programID, "a_offset");
    int32_t colorLocation  = ctx.getAttribLocation(programID, "a_color");

    float cellW                       = 2.0f / (float)m_gridSide;
    float cellH                       = 2.0f / (float)m_gridSide;
    const tcu::Vec4 vertexPositions[] = {
        tcu::Vec4(0, 0, 0, 1),     tcu::Vec4(cellW, 0, 0, 1), tcu::Vec4(0, cellH, 0, 1),

        tcu::Vec4(0, cellH, 0, 1), tcu::Vec4(cellW, 0, 0, 1), tcu::Vec4(cellW, cellH, 0, 1),
    };

    const uint16_t indices[] = {0, 4, 3, 2, 1, 5};

    std::vector<tcu::Vec4> offsets;
    for (int x = 0; x < m_gridSide; ++x)
        for (int y = 0; y < m_gridSide; ++y)
            offsets.push_back(tcu::Vec4((float)x * cellW - 1.0f, (float)y * cellW - 1.0f, 0, 0));

    std::vector<tcu::Vec4> colors;
    for (int x = 0; x < m_gridSide; ++x)
        for (int y = 0; y < m_gridSide; ++y)
            colors.push_back(((x + y) % 2 == 0) ? (green) : (yellow));

    ctx.genBuffers(1, &positionBuf);
    ctx.bindBuffer(GL_ARRAY_BUFFER, positionBuf);
    ctx.bufferData(GL_ARRAY_BUFFER, sizeof(vertexPositions), vertexPositions, GL_STATIC_DRAW);
    ctx.vertexAttribPointer(posLocation, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    ctx.vertexAttribDivisor(posLocation, 0);
    ctx.enableVertexAttribArray(posLocation);

    ctx.genBuffers(1, &offsetBuf);
    ctx.bindBuffer(GL_ARRAY_BUFFER, offsetBuf);
    ctx.bufferData(GL_ARRAY_BUFFER, offsets.size() * sizeof(tcu::Vec4), &offsets[0], GL_STATIC_DRAW);
    ctx.vertexAttribPointer(offsetLocation, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    ctx.vertexAttribDivisor(offsetLocation, 1);
    ctx.enableVertexAttribArray(offsetLocation);

    ctx.genBuffers(1, &colorBuf);
    ctx.bindBuffer(GL_ARRAY_BUFFER, colorBuf);
    ctx.bufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(tcu::Vec4), &colors[0], GL_STATIC_DRAW);
    ctx.vertexAttribPointer(colorLocation, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    ctx.vertexAttribDivisor(colorLocation, 1);
    ctx.enableVertexAttribArray(colorLocation);

    if (m_useIndices)
    {
        ctx.genBuffers(1, &indexBuf);
        ctx.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuf);
        ctx.bufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    }

    ctx.clearColor(0, 0, 0, 1);
    ctx.clear(GL_COLOR_BUFFER_BIT);

    ctx.viewport(0, 0, dst.getWidth(), dst.getHeight());

    ctx.useProgram(programID);
    if (m_useIndices)
        ctx.drawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr, m_gridSide * m_gridSide);
    else
        ctx.drawArraysInstanced(GL_TRIANGLES, 0, 6, m_gridSide * m_gridSide);
    ctx.useProgram(0);

    if (m_useIndices)
        ctx.deleteBuffers(1, &indexBuf);
    ctx.deleteBuffers(1, &colorBuf);
    ctx.deleteBuffers(1, &offsetBuf);
    ctx.deleteBuffers(1, &positionBuf);
    ctx.deleteProgram(programID);

    ctx.finish();
    ctx.readPixels(dst, 0, 0, dst.getWidth(), dst.getHeight());

    glu::checkError(ctx.getError(), "", __FILE__, __LINE__);
}

bool InstancedGridRenderTest::verifyImage(const tcu::Surface &image)
{
    // \note the green/yellow pattern is only for clarity. The test will only verify that all instances were drawn by looking for anything non-green/yellow.
    using tcu::TestLog;

    const int colorThreshold = 20;

    tcu::Surface error(image.getWidth(), image.getHeight());
    bool isOk = true;

    for (int y = 0; y < image.getHeight(); y++)
        for (int x = 0; x < image.getWidth(); x++)
        {
            if (x == 0 || y == 0 || y + 1 == image.getHeight() || x + 1 == image.getWidth())
            {
                // Background color might bleed in at the borders with msaa
                error.setPixel(x, y, tcu::RGBA(0, 255, 0, 255));
            }
            else
            {
                const tcu::RGBA pixel = image.getPixel(x, y);
                bool pixelOk          = true;

                // Any pixel with !(G ~= 255) is faulty (not a linear combinations of green and yellow)
                if (de::abs(pixel.getGreen() - 255) > colorThreshold)
                    pixelOk = false;

                // Any pixel with !(B ~= 0) is faulty (not a linear combinations of green and yellow)
                if (de::abs(pixel.getBlue() - 0) > colorThreshold)
                    pixelOk = false;

                error.setPixel(x, y, (pixelOk) ? (tcu::RGBA(0, 255, 0, 255)) : (tcu::RGBA(255, 0, 0, 255)));
                isOk = isOk && pixelOk;
            }
        }

    if (!isOk)
    {
        tcu::TestLog &log = m_testCtx.getLog();

        log << TestLog::Message << "Image verification failed." << TestLog::EndMessage;
        log << TestLog::ImageSet("Verfication result", "Result of rendering")
            << TestLog::Image("Result", "Result", image) << TestLog::Image("ErrorMask", "Error mask", error)
            << TestLog::EndImageSet;
    }
    else
    {
        tcu::TestLog &log = m_testCtx.getLog();

        log << TestLog::ImageSet("Verfication result", "Result of rendering")
            << TestLog::Image("Result", "Result", image) << TestLog::EndImageSet;
    }

    return isOk;
}

class InstancingGroup : public TestCaseGroup
{
public:
    InstancingGroup(Context &context, const char *name, const char *descr);
    ~InstancingGroup(void);

    void init(void);
};

InstancingGroup::InstancingGroup(Context &context, const char *name, const char *descr)
    : TestCaseGroup(context, name, descr)
{
}

InstancingGroup::~InstancingGroup(void)
{
}

void InstancingGroup::init(void)
{
    const int gridWidths[] = {
        2, 5, 10, 32, 100,
    };

    // drawArrays
    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(gridWidths); ++ndx)
    {
        const std::string name = std::string("draw_arrays_instanced_grid_") + de::toString(gridWidths[ndx]) + "x" +
                                 de::toString(gridWidths[ndx]);
        const std::string desc = std::string("DrawArraysInstanced, Grid size ") + de::toString(gridWidths[ndx]) + "x" +
                                 de::toString(gridWidths[ndx]);

        this->addChild(new InstancedGridRenderTest(m_context, name.c_str(), desc.c_str(), gridWidths[ndx], false));
    }

    // drawElements
    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(gridWidths); ++ndx)
    {
        const std::string name = std::string("draw_elements_instanced_grid_") + de::toString(gridWidths[ndx]) + "x" +
                                 de::toString(gridWidths[ndx]);
        const std::string desc = std::string("DrawElementsInstanced, Grid size ") + de::toString(gridWidths[ndx]) +
                                 "x" + de::toString(gridWidths[ndx]);

        this->addChild(new InstancedGridRenderTest(m_context, name.c_str(), desc.c_str(), gridWidths[ndx], true));
    }
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

    static const int attribCounts[]             = {1, 2, 5};
    static const float attribWeights[]          = {30, 10, 1};
    static const int primitiveCounts[]          = {1, 5, 64};
    static const float primitiveCountWeights[]  = {20, 10, 1};
    static const int indexOffsets[]             = {0, 7, 13};
    static const float indexOffsetWeights[]     = {20, 20, 1};
    static const int firsts[]                   = {0, 7, 13};
    static const float firstWeights[]           = {20, 20, 1};
    static const int instanceCounts[]           = {1, 2, 16, 17};
    static const float instanceWeights[]        = {20, 10, 5, 1};
    static const int indexMins[]                = {0, 1, 3, 8};
    static const int indexMaxs[]                = {4, 8, 128, 257};
    static const float indexWeights[]           = {50, 50, 50, 50};
    static const int offsets[]                  = {0, 1, 5, 12};
    static const float offsetWeights[]          = {50, 10, 10, 10};
    static const int strides[]                  = {0, 7, 16, 17};
    static const float strideWeights[]          = {50, 10, 10, 10};
    static const int instanceDivisors[]         = {0, 1, 3, 129};
    static const float instanceDivisorWeights[] = {70, 30, 10, 10};

    static const gls::DrawTestSpec::Primitive primitives[] = {
        gls::DrawTestSpec::PRIMITIVE_POINTS,       gls::DrawTestSpec::PRIMITIVE_TRIANGLES,
        gls::DrawTestSpec::PRIMITIVE_TRIANGLE_FAN, gls::DrawTestSpec::PRIMITIVE_TRIANGLE_STRIP,
        gls::DrawTestSpec::PRIMITIVE_LINES,        gls::DrawTestSpec::PRIMITIVE_LINE_STRIP,
        gls::DrawTestSpec::PRIMITIVE_LINE_LOOP};
    const UniformWeightArray<DE_LENGTH_OF_ARRAY(primitives)> primitiveWeights;

    static const gls::DrawTestSpec::DrawMethod drawMethods[] = {
        gls::DrawTestSpec::DRAWMETHOD_DRAWARRAYS, gls::DrawTestSpec::DRAWMETHOD_DRAWARRAYS_INSTANCED,
        gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS, gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_RANGED,
        gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_INSTANCED};
    const UniformWeightArray<DE_LENGTH_OF_ARRAY(drawMethods)> drawMethodWeights;

    static const gls::DrawTestSpec::IndexType indexTypes[] = {
        gls::DrawTestSpec::INDEXTYPE_BYTE,
        gls::DrawTestSpec::INDEXTYPE_SHORT,
        gls::DrawTestSpec::INDEXTYPE_INT,
    };
    const UniformWeightArray<DE_LENGTH_OF_ARRAY(indexTypes)> indexTypeWeights;

    static const gls::DrawTestSpec::Storage storages[] = {
        gls::DrawTestSpec::STORAGE_USER,
        gls::DrawTestSpec::STORAGE_BUFFER,
    };
    const UniformWeightArray<DE_LENGTH_OF_ARRAY(storages)> storageWeights;

    static const gls::DrawTestSpec::InputType inputTypes[] = {
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

    static const gls::DrawTestSpec::OutputType outputTypes[] = {
        gls::DrawTestSpec::OUTPUTTYPE_FLOAT, gls::DrawTestSpec::OUTPUTTYPE_VEC2,  gls::DrawTestSpec::OUTPUTTYPE_VEC3,
        gls::DrawTestSpec::OUTPUTTYPE_VEC4,  gls::DrawTestSpec::OUTPUTTYPE_INT,   gls::DrawTestSpec::OUTPUTTYPE_UINT,
        gls::DrawTestSpec::OUTPUTTYPE_IVEC2, gls::DrawTestSpec::OUTPUTTYPE_IVEC3, gls::DrawTestSpec::OUTPUTTYPE_IVEC4,
        gls::DrawTestSpec::OUTPUTTYPE_UVEC2, gls::DrawTestSpec::OUTPUTTYPE_UVEC3, gls::DrawTestSpec::OUTPUTTYPE_UVEC4,
    };
    const UniformWeightArray<DE_LENGTH_OF_ARRAY(outputTypes)> outputTypeWeights;

    static const gls::DrawTestSpec::Usage usages[] = {
        gls::DrawTestSpec::USAGE_DYNAMIC_DRAW, gls::DrawTestSpec::USAGE_STATIC_DRAW,
        gls::DrawTestSpec::USAGE_STREAM_DRAW,  gls::DrawTestSpec::USAGE_STREAM_READ,
        gls::DrawTestSpec::USAGE_STREAM_COPY,  gls::DrawTestSpec::USAGE_STATIC_READ,
        gls::DrawTestSpec::USAGE_STATIC_COPY,  gls::DrawTestSpec::USAGE_DYNAMIC_READ,
        gls::DrawTestSpec::USAGE_DYNAMIC_COPY,
    };
    const UniformWeightArray<DE_LENGTH_OF_ARRAY(usages)> usageWeights;

    static const uint32_t disallowedCases[] = {
        544, //!< extremely narrow triangle
    };

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
                // Only properly aligned and not disallowed cases
                if (spec.isCompatibilityTest() != gls::DrawTestSpec::COMPATIBILITY_UNALIGNED_OFFSET &&
                    spec.isCompatibilityTest() != gls::DrawTestSpec::COMPATIBILITY_UNALIGNED_STRIDE &&
                    !de::contains(DE_ARRAY_BEGIN(disallowedCases), DE_ARRAY_END(disallowedCases), hash))
                {
                    this->addChild(new gls::DrawTest(m_testCtx, m_context.getRenderContext(), spec,
                                                     de::toString(insertedCount).c_str(), spec.getDesc().c_str()));
                }
                insertedHashes.insert(hash);

                ++insertedCount;
            }
        }
    }
}

} // namespace

DrawTests::DrawTests(Context &context) : TestCaseGroup(context, "draw", "Drawing tests")
{
}

DrawTests::~DrawTests(void)
{
}

void DrawTests::init(void)
{
    // Basic
    {
        const gls::DrawTestSpec::DrawMethod basicMethods[] = {
            gls::DrawTestSpec::DRAWMETHOD_DRAWARRAYS,           gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS,
            gls::DrawTestSpec::DRAWMETHOD_DRAWARRAYS_INSTANCED, gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_INSTANCED,
            gls::DrawTestSpec::DRAWMETHOD_DRAWELEMENTS_RANGED,
        };

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(basicMethods); ++ndx)
        {
            const std::string name = gls::DrawTestSpec::drawMethodToString(basicMethods[ndx]);
            const std::string desc = gls::DrawTestSpec::drawMethodToString(basicMethods[ndx]);

            this->addChild(new MethodGroup(m_context, name.c_str(), desc.c_str(), basicMethods[ndx]));
        }
    }

    // extreme instancing

    this->addChild(new InstancingGroup(m_context, "instancing", "draw tests with a large instance count."));

    // Random

    this->addChild(new RandomGroup(m_context, "random", "random draw commands."));
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
