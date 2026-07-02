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
 * \brief Vertex array and buffer unaligned access stress tests
 *//*--------------------------------------------------------------------*/

#include "es3sVertexArrayTests.hpp"
#include "glsVertexArrayTests.hpp"

#include <sstream>

using namespace deqp::gls;

namespace deqp
{
namespace gles3
{
namespace Stress
{
namespace
{

class SingleVertexArrayUsageGroup : public TestCaseGroup
{
public:
    SingleVertexArrayUsageGroup(Context &context, Array::Usage usage);
    virtual ~SingleVertexArrayUsageGroup(void);

    virtual void init(void);

private:
    SingleVertexArrayUsageGroup(const SingleVertexArrayUsageGroup &other);
    SingleVertexArrayUsageGroup &operator=(const SingleVertexArrayUsageGroup &other);

    Array::Usage m_usage;
};

SingleVertexArrayUsageGroup::SingleVertexArrayUsageGroup(Context &context, Array::Usage usage)
    : TestCaseGroup(context, Array::usageTypeToString(usage).c_str(), Array::usageTypeToString(usage).c_str())
    , m_usage(usage)
{
}

SingleVertexArrayUsageGroup::~SingleVertexArrayUsageGroup(void)
{
}

template <class T>
static std::string typeToString(T t)
{
    std::stringstream strm;
    strm << t;
    return strm.str();
}

void SingleVertexArrayUsageGroup::init(void)
{
    int counts[]  = {1, 256};
    int strides[] = {0, -1, 17, 32}; // Tread negative value as sizeof input. Same as 0, but done outside of GL.
    Array::InputType inputTypes[] = {Array::INPUTTYPE_FLOAT, Array::INPUTTYPE_FIXED, Array::INPUTTYPE_SHORT,
                                     Array::INPUTTYPE_BYTE};

    for (int inputTypeNdx = 0; inputTypeNdx < DE_LENGTH_OF_ARRAY(inputTypes); inputTypeNdx++)
    {
        for (int countNdx = 0; countNdx < DE_LENGTH_OF_ARRAY(counts); countNdx++)
        {
            for (int strideNdx = 0; strideNdx < DE_LENGTH_OF_ARRAY(strides); strideNdx++)
            {
                const int stride =
                    (strides[strideNdx] < 0 ? Array::inputTypeSize(inputTypes[inputTypeNdx]) * 2 : strides[strideNdx]);
                const bool aligned     = (stride % Array::inputTypeSize(inputTypes[inputTypeNdx])) == 0;
                const std::string name = "stride" + typeToString(stride) + "_" +
                                         Array::inputTypeToString(inputTypes[inputTypeNdx]) + "_quads" +
                                         typeToString(counts[countNdx]);

                MultiVertexArrayTest::Spec::ArraySpec arraySpec(inputTypes[inputTypeNdx], Array::OUTPUTTYPE_VEC2,
                                                                Array::STORAGE_BUFFER, m_usage, 2, 0, stride, false,
                                                                GLValue::getMinValue(inputTypes[inputTypeNdx]),
                                                                GLValue::getMaxValue(inputTypes[inputTypeNdx]));

                MultiVertexArrayTest::Spec spec;
                spec.primitive = Array::PRIMITIVE_TRIANGLES;
                spec.drawCount = counts[countNdx];
                spec.first     = 0;
                spec.arrays.push_back(arraySpec);

                if (!aligned)
                    addChild(new MultiVertexArrayTest(m_testCtx, m_context.getRenderContext(), spec, name.c_str(),
                                                      name.c_str()));
            }
        }
    }
}

class SingleVertexArrayUsageTests : public TestCaseGroup
{
public:
    SingleVertexArrayUsageTests(Context &context);
    virtual ~SingleVertexArrayUsageTests(void);

    virtual void init(void);

private:
    SingleVertexArrayUsageTests(const SingleVertexArrayUsageTests &other);
    SingleVertexArrayUsageTests &operator=(const SingleVertexArrayUsageTests &other);
};

SingleVertexArrayUsageTests::SingleVertexArrayUsageTests(Context &context)
    : TestCaseGroup(context, "usages", "Single vertex atribute, usage")
{
}

SingleVertexArrayUsageTests::~SingleVertexArrayUsageTests(void)
{
}

void SingleVertexArrayUsageTests::init(void)
{
    // Test usage
    Array::Usage usages[] = {Array::USAGE_STATIC_DRAW, Array::USAGE_STREAM_DRAW, Array::USAGE_DYNAMIC_DRAW,
                             Array::USAGE_STATIC_COPY, Array::USAGE_STREAM_COPY, Array::USAGE_DYNAMIC_COPY,
                             Array::USAGE_STATIC_READ, Array::USAGE_STREAM_READ, Array::USAGE_DYNAMIC_READ};
    for (int usageNdx = 0; usageNdx < DE_LENGTH_OF_ARRAY(usages); usageNdx++)
    {
        addChild(new SingleVertexArrayUsageGroup(m_context, usages[usageNdx]));
    }
}

class SingleVertexArrayStrideGroup : public TestCaseGroup
{
public:
    SingleVertexArrayStrideGroup(Context &context, Array::InputType type);
    virtual ~SingleVertexArrayStrideGroup(void);

    virtual void init(void);

private:
    SingleVertexArrayStrideGroup(const SingleVertexArrayStrideGroup &other);
    SingleVertexArrayStrideGroup &operator=(const SingleVertexArrayStrideGroup &other);

    Array::InputType m_type;
};

SingleVertexArrayStrideGroup::SingleVertexArrayStrideGroup(Context &context, Array::InputType type)
    : TestCaseGroup(context, Array::inputTypeToString(type).c_str(), Array::inputTypeToString(type).c_str())
    , m_type(type)
{
}

SingleVertexArrayStrideGroup::~SingleVertexArrayStrideGroup(void)
{
}

void SingleVertexArrayStrideGroup::init(void)
{
    Array::Storage storages[] = {Array::STORAGE_USER, Array::STORAGE_BUFFER};
    int counts[]              = {1, 256};
    int strides[] = {/*0,*/ -1, 17, 32}; // Tread negative value as sizeof input. Same as 0, but done outside of GL.

    for (int storageNdx = 0; storageNdx < DE_LENGTH_OF_ARRAY(storages); storageNdx++)
    {
        for (int componentCount = 2; componentCount < 5; componentCount++)
        {
            for (int countNdx = 0; countNdx < DE_LENGTH_OF_ARRAY(counts); countNdx++)
            {
                for (int strideNdx = 0; strideNdx < DE_LENGTH_OF_ARRAY(strides); strideNdx++)
                {
                    const bool packed =
                        m_type == Array::INPUTTYPE_UNSIGNED_INT_2_10_10_10 || m_type == Array::INPUTTYPE_INT_2_10_10_10;
                    const int stride = (strides[strideNdx] < 0) ?
                                           ((packed) ? (16) : (Array::inputTypeSize(m_type) * componentCount)) :
                                           (strides[strideNdx]);
                    const int alignment =
                        (packed) ? (Array::inputTypeSize(m_type) * componentCount) : (Array::inputTypeSize(m_type));
                    const bool bufferUnaligned =
                        (storages[storageNdx] == Array::STORAGE_BUFFER) && (stride % alignment) != 0;

                    std::string name = Array::storageToString(storages[storageNdx]) + "_stride" + typeToString(stride) +
                                       "_components" + typeToString(componentCount) + "_quads" +
                                       typeToString(counts[countNdx]);

                    if ((m_type == Array::INPUTTYPE_UNSIGNED_INT_2_10_10_10 ||
                         m_type == Array::INPUTTYPE_INT_2_10_10_10) &&
                        componentCount != 4)
                        continue;

                    MultiVertexArrayTest::Spec::ArraySpec arraySpec(
                        m_type, Array::OUTPUTTYPE_VEC4, storages[storageNdx], Array::USAGE_DYNAMIC_DRAW, componentCount,
                        0, stride, false, GLValue::getMinValue(m_type), GLValue::getMaxValue(m_type));

                    MultiVertexArrayTest::Spec spec;
                    spec.primitive = Array::PRIMITIVE_TRIANGLES;
                    spec.drawCount = counts[countNdx];
                    spec.first     = 0;
                    spec.arrays.push_back(arraySpec);

                    if (bufferUnaligned)
                        addChild(new MultiVertexArrayTest(m_testCtx, m_context.getRenderContext(), spec, name.c_str(),
                                                          name.c_str()));
                }
            }
        }
    }
}

class SingleVertexArrayStrideTests : public TestCaseGroup
{
public:
    SingleVertexArrayStrideTests(Context &context);
    virtual ~SingleVertexArrayStrideTests(void);

    virtual void init(void);

private:
    SingleVertexArrayStrideTests(const SingleVertexArrayStrideTests &other);
    SingleVertexArrayStrideTests &operator=(const SingleVertexArrayStrideTests &other);
};

SingleVertexArrayStrideTests::SingleVertexArrayStrideTests(Context &context)
    : TestCaseGroup(context, "strides", "Single stride vertex atribute")
{
}

SingleVertexArrayStrideTests::~SingleVertexArrayStrideTests(void)
{
}

void SingleVertexArrayStrideTests::init(void)
{
    Array::InputType inputTypes[] = {
        Array::INPUTTYPE_FLOAT, Array::INPUTTYPE_SHORT,
        /*Array::INPUTTYPE_UNSIGNED_SHORT, Array::INPUTTYPE_UNSIGNED_BYTE,*/ Array::INPUTTYPE_FIXED,
        Array::INPUTTYPE_INT_2_10_10_10};

    for (int inputTypeNdx = 0; inputTypeNdx < DE_LENGTH_OF_ARRAY(inputTypes); inputTypeNdx++)
    {
        addChild(new SingleVertexArrayStrideGroup(m_context, inputTypes[inputTypeNdx]));
    }
}

class SingleVertexArrayFirstGroup : public TestCaseGroup
{
public:
    SingleVertexArrayFirstGroup(Context &context, Array::InputType type);
    virtual ~SingleVertexArrayFirstGroup(void);

    virtual void init(void);

private:
    SingleVertexArrayFirstGroup(const SingleVertexArrayFirstGroup &other);
    SingleVertexArrayFirstGroup &operator=(const SingleVertexArrayFirstGroup &other);
    Array::InputType m_type;
};

SingleVertexArrayFirstGroup::SingleVertexArrayFirstGroup(Context &context, Array::InputType type)
    : TestCaseGroup(context, Array::inputTypeToString(type).c_str(), Array::inputTypeToString(type).c_str())
    , m_type(type)
{
}

SingleVertexArrayFirstGroup::~SingleVertexArrayFirstGroup(void)
{
}

void SingleVertexArrayFirstGroup::init(void)
{
    int counts[]  = {5, 256};
    int firsts[]  = {6, 24};
    int offsets[] = {1, 17};
    int strides[] = {/*0,*/ -1, 17, 32}; // Tread negative value as sizeof input. Same as 0, but done outside of GL.

    for (int offsetNdx = 0; offsetNdx < DE_LENGTH_OF_ARRAY(offsets); offsetNdx++)
    {
        for (int countNdx = 0; countNdx < DE_LENGTH_OF_ARRAY(counts); countNdx++)
        {
            for (int strideNdx = 0; strideNdx < DE_LENGTH_OF_ARRAY(strides); strideNdx++)
            {
                for (int firstNdx = 0; firstNdx < DE_LENGTH_OF_ARRAY(firsts); firstNdx++)
                {
                    const bool packed =
                        m_type == Array::INPUTTYPE_UNSIGNED_INT_2_10_10_10 || m_type == Array::INPUTTYPE_INT_2_10_10_10;
                    const int componentCount = (packed) ? (4) : (2);
                    const int stride         = (strides[strideNdx] < 0) ?
                                                   ((packed) ? (8) : (Array::inputTypeSize(m_type) * componentCount)) :
                                                   (strides[strideNdx]);
                    const int alignment =
                        (packed) ? (Array::inputTypeSize(m_type) * componentCount) : (Array::inputTypeSize(m_type));
                    const bool aligned = ((stride % alignment) == 0) && ((offsets[offsetNdx] % alignment) == 0);
                    std::string name   = "first" + typeToString(firsts[firstNdx]) + "_offset" +
                                       typeToString(offsets[offsetNdx]) + "_stride" + typeToString(stride) + "_quads" +
                                       typeToString(counts[countNdx]);

                    MultiVertexArrayTest::Spec::ArraySpec arraySpec(
                        m_type, Array::OUTPUTTYPE_VEC2, Array::STORAGE_BUFFER, Array::USAGE_DYNAMIC_DRAW,
                        componentCount, offsets[offsetNdx], stride, false, GLValue::getMinValue(m_type),
                        GLValue::getMaxValue(m_type));

                    MultiVertexArrayTest::Spec spec;
                    spec.primitive = Array::PRIMITIVE_TRIANGLES;
                    spec.drawCount = counts[countNdx];
                    spec.first     = firsts[firstNdx];
                    spec.arrays.push_back(arraySpec);

                    if (!aligned)
                        addChild(new MultiVertexArrayTest(m_testCtx, m_context.getRenderContext(), spec, name.c_str(),
                                                          name.c_str()));
                }
            }
        }
    }
}

class SingleVertexArrayFirstTests : public TestCaseGroup
{
public:
    SingleVertexArrayFirstTests(Context &context);
    virtual ~SingleVertexArrayFirstTests(void);

    virtual void init(void);

private:
    SingleVertexArrayFirstTests(const SingleVertexArrayFirstTests &other);
    SingleVertexArrayFirstTests &operator=(const SingleVertexArrayFirstTests &other);
};

SingleVertexArrayFirstTests::SingleVertexArrayFirstTests(Context &context)
    : TestCaseGroup(context, "first", "Single vertex attribute, different first values to drawArrays")
{
}

SingleVertexArrayFirstTests::~SingleVertexArrayFirstTests(void)
{
}

void SingleVertexArrayFirstTests::init(void)
{
    // Test offset with different input types, component counts and storage, Usage(?)
    Array::InputType inputTypes[] = {Array::INPUTTYPE_FLOAT, Array::INPUTTYPE_INT_2_10_10_10};

    for (int inputTypeNdx = 0; inputTypeNdx < DE_LENGTH_OF_ARRAY(inputTypes); inputTypeNdx++)
    {
        addChild(new SingleVertexArrayFirstGroup(m_context, inputTypes[inputTypeNdx]));
    }
}

class SingleVertexArrayOffsetGroup : public TestCaseGroup
{
public:
    SingleVertexArrayOffsetGroup(Context &context, Array::InputType type);
    virtual ~SingleVertexArrayOffsetGroup(void);

    virtual void init(void);

private:
    SingleVertexArrayOffsetGroup(const SingleVertexArrayOffsetGroup &other);
    SingleVertexArrayOffsetGroup &operator=(const SingleVertexArrayOffsetGroup &other);
    Array::InputType m_type;
};

SingleVertexArrayOffsetGroup::SingleVertexArrayOffsetGroup(Context &context, Array::InputType type)
    : TestCaseGroup(context, Array::inputTypeToString(type).c_str(), Array::inputTypeToString(type).c_str())
    , m_type(type)
{
}

SingleVertexArrayOffsetGroup::~SingleVertexArrayOffsetGroup(void)
{
}

void SingleVertexArrayOffsetGroup::init(void)
{
    int counts[]  = {1, 256};
    int offsets[] = {1, 4, 17, 32};
    int strides[] = {/*0,*/ -1, 17, 32}; // Tread negative value as sizeof input. Same as 0, but done outside of GL.

    for (int offsetNdx = 0; offsetNdx < DE_LENGTH_OF_ARRAY(offsets); offsetNdx++)
    {
        for (int countNdx = 0; countNdx < DE_LENGTH_OF_ARRAY(counts); countNdx++)
        {
            for (int strideNdx = 0; strideNdx < DE_LENGTH_OF_ARRAY(strides); strideNdx++)
            {
                const bool packed =
                    m_type == Array::INPUTTYPE_UNSIGNED_INT_2_10_10_10 || m_type == Array::INPUTTYPE_INT_2_10_10_10;
                const int componentCount = (packed) ? (4) : (2);
                const int stride =
                    (strides[strideNdx] < 0 ? Array::inputTypeSize(m_type) * componentCount : strides[strideNdx]);
                const int alignment =
                    (packed) ? (Array::inputTypeSize(m_type) * componentCount) : (Array::inputTypeSize(m_type));
                const bool aligned     = ((stride % alignment) == 0) && ((offsets[offsetNdx] % alignment) == 0);
                const std::string name = "offset" + typeToString(offsets[offsetNdx]) + "_stride" +
                                         typeToString(strides[strideNdx]) + "_quads" + typeToString(counts[countNdx]);

                MultiVertexArrayTest::Spec::ArraySpec arraySpec(
                    m_type, Array::OUTPUTTYPE_VEC2, Array::STORAGE_BUFFER, Array::USAGE_DYNAMIC_DRAW, componentCount,
                    offsets[offsetNdx], stride, false, GLValue::getMinValue(m_type), GLValue::getMaxValue(m_type));

                MultiVertexArrayTest::Spec spec;
                spec.primitive = Array::PRIMITIVE_TRIANGLES;
                spec.drawCount = counts[countNdx];
                spec.first     = 0;
                spec.arrays.push_back(arraySpec);

                if (!aligned)
                    addChild(new MultiVertexArrayTest(m_testCtx, m_context.getRenderContext(), spec, name.c_str(),
                                                      name.c_str()));
            }
        }
    }
}

class SingleVertexArrayOffsetTests : public TestCaseGroup
{
public:
    SingleVertexArrayOffsetTests(Context &context);
    virtual ~SingleVertexArrayOffsetTests(void);

    virtual void init(void);

private:
    SingleVertexArrayOffsetTests(const SingleVertexArrayOffsetTests &other);
    SingleVertexArrayOffsetTests &operator=(const SingleVertexArrayOffsetTests &other);
};

SingleVertexArrayOffsetTests::SingleVertexArrayOffsetTests(Context &context)
    : TestCaseGroup(context, "offset", "Single vertex atribute offset element")
{
}

SingleVertexArrayOffsetTests::~SingleVertexArrayOffsetTests(void)
{
}

void SingleVertexArrayOffsetTests::init(void)
{
    // Test offset with different input types, component counts and storage, Usage(?)
    Array::InputType inputTypes[] = {Array::INPUTTYPE_FLOAT, Array::INPUTTYPE_INT_2_10_10_10};

    for (int inputTypeNdx = 0; inputTypeNdx < DE_LENGTH_OF_ARRAY(inputTypes); inputTypeNdx++)
    {
        addChild(new SingleVertexArrayOffsetGroup(m_context, inputTypes[inputTypeNdx]));
    }
}

} // namespace

VertexArrayTests::VertexArrayTests(Context &context)
    : TestCaseGroup(context, "vertex_arrays", "Vertex array and array tests")
{
}

VertexArrayTests::~VertexArrayTests(void)
{
}

void VertexArrayTests::init(void)
{
    tcu::TestCaseGroup *const group = new tcu::TestCaseGroup(m_testCtx, "single_attribute", "Single attribute");
    addChild(group);

    // .single_attribute
    {
        group->addChild(new SingleVertexArrayStrideTests(m_context));
        group->addChild(new SingleVertexArrayUsageTests(m_context));
        group->addChild(new SingleVertexArrayOffsetTests(m_context));
        group->addChild(new SingleVertexArrayFirstTests(m_context));
    }
}

} // namespace Stress
} // namespace gles3
} // namespace deqp
