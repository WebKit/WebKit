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
 * \brief Vertex array and buffer tests
 *//*--------------------------------------------------------------------*/

#include "es3fVertexArrayTest.hpp"
#include "glsVertexArrayTests.hpp"

#include <sstream>

using namespace deqp::gls;

namespace deqp
{
namespace gles3
{
namespace Functional
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

                if (aligned)
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

                    if (!bufferUnaligned)
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
        Array::INPUTTYPE_FLOAT, Array::INPUTTYPE_SHORT, Array::INPUTTYPE_BYTE,
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
    int offsets[] = {1, 16, 17};
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

                    if (aligned)
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
    Array::InputType inputTypes[] = {Array::INPUTTYPE_FLOAT, Array::INPUTTYPE_BYTE, Array::INPUTTYPE_INT_2_10_10_10};

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
                                         typeToString(stride) + "_quads" + typeToString(counts[countNdx]);

                MultiVertexArrayTest::Spec::ArraySpec arraySpec(
                    m_type, Array::OUTPUTTYPE_VEC2, Array::STORAGE_BUFFER, Array::USAGE_DYNAMIC_DRAW, componentCount,
                    offsets[offsetNdx], stride, false, GLValue::getMinValue(m_type), GLValue::getMaxValue(m_type));

                MultiVertexArrayTest::Spec spec;
                spec.primitive = Array::PRIMITIVE_TRIANGLES;
                spec.drawCount = counts[countNdx];
                spec.first     = 0;
                spec.arrays.push_back(arraySpec);

                if (aligned)
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
    Array::InputType inputTypes[] = {Array::INPUTTYPE_FLOAT, Array::INPUTTYPE_BYTE, Array::INPUTTYPE_INT_2_10_10_10};

    for (int inputTypeNdx = 0; inputTypeNdx < DE_LENGTH_OF_ARRAY(inputTypes); inputTypeNdx++)
    {
        addChild(new SingleVertexArrayOffsetGroup(m_context, inputTypes[inputTypeNdx]));
    }
}

class SingleVertexArrayNormalizeGroup : public TestCaseGroup
{
public:
    SingleVertexArrayNormalizeGroup(Context &context, Array::InputType type);
    virtual ~SingleVertexArrayNormalizeGroup(void);

    virtual void init(void);

private:
    SingleVertexArrayNormalizeGroup(const SingleVertexArrayNormalizeGroup &other);
    SingleVertexArrayNormalizeGroup &operator=(const SingleVertexArrayNormalizeGroup &other);
    Array::InputType m_type;
};

SingleVertexArrayNormalizeGroup::SingleVertexArrayNormalizeGroup(Context &context, Array::InputType type)
    : TestCaseGroup(context, Array::inputTypeToString(type).c_str(), Array::inputTypeToString(type).c_str())
    , m_type(type)
{
}

SingleVertexArrayNormalizeGroup::~SingleVertexArrayNormalizeGroup(void)
{
}

void SingleVertexArrayNormalizeGroup::init(void)
{
    int counts[] = {1, 256};

    for (int componentCount = 2; componentCount < 5; componentCount++)
    {
        for (int countNdx = 0; countNdx < DE_LENGTH_OF_ARRAY(counts); countNdx++)
        {
            if ((m_type == Array::INPUTTYPE_UNSIGNED_INT_2_10_10_10 || m_type == Array::INPUTTYPE_INT_2_10_10_10) &&
                componentCount != 4)
                continue;

            std::string name = "components" + typeToString(componentCount) + "_quads" + typeToString(counts[countNdx]);

            MultiVertexArrayTest::Spec::ArraySpec arraySpec(m_type, Array::OUTPUTTYPE_VEC4, Array::STORAGE_USER,
                                                            Array::USAGE_DYNAMIC_DRAW, componentCount, 0, 0, true,
                                                            GLValue::getMinValue(m_type), GLValue::getMaxValue(m_type));

            MultiVertexArrayTest::Spec spec;
            spec.primitive = Array::PRIMITIVE_TRIANGLES;
            spec.drawCount = counts[countNdx];
            spec.first     = 0;
            spec.arrays.push_back(arraySpec);

            addChild(
                new MultiVertexArrayTest(m_testCtx, m_context.getRenderContext(), spec, name.c_str(), name.c_str()));
        }
    }
}

class SingleVertexArrayNormalizeTests : public TestCaseGroup
{
public:
    SingleVertexArrayNormalizeTests(Context &context);
    virtual ~SingleVertexArrayNormalizeTests(void);

    virtual void init(void);

private:
    SingleVertexArrayNormalizeTests(const SingleVertexArrayNormalizeTests &other);
    SingleVertexArrayNormalizeTests &operator=(const SingleVertexArrayNormalizeTests &other);
};

SingleVertexArrayNormalizeTests::SingleVertexArrayNormalizeTests(Context &context)
    : TestCaseGroup(context, "normalize", "Single normalize vertex atribute")
{
}

SingleVertexArrayNormalizeTests::~SingleVertexArrayNormalizeTests(void)
{
}

void SingleVertexArrayNormalizeTests::init(void)
{
    // Test normalization with different input types, component counts and storage
    Array::InputType inputTypes[] = {Array::INPUTTYPE_FLOAT,         Array::INPUTTYPE_SHORT,
                                     Array::INPUTTYPE_BYTE,          Array::INPUTTYPE_UNSIGNED_SHORT,
                                     Array::INPUTTYPE_UNSIGNED_BYTE, Array::INPUTTYPE_FIXED,
                                     Array::INPUTTYPE_UNSIGNED_INT,  Array::INPUTTYPE_INT,
                                     Array::INPUTTYPE_HALF,          Array::INPUTTYPE_UNSIGNED_INT_2_10_10_10,
                                     Array::INPUTTYPE_INT_2_10_10_10};

    for (int inputTypeNdx = 0; inputTypeNdx < DE_LENGTH_OF_ARRAY(inputTypes); inputTypeNdx++)
    {
        addChild(new SingleVertexArrayNormalizeGroup(m_context, inputTypes[inputTypeNdx]));
    }
}

class SingleVertexArrayOutputTypeGroup : public TestCaseGroup
{
public:
    SingleVertexArrayOutputTypeGroup(Context &context, Array::InputType type);
    virtual ~SingleVertexArrayOutputTypeGroup(void);

    virtual void init(void);

private:
    SingleVertexArrayOutputTypeGroup(const SingleVertexArrayOutputTypeGroup &other);
    SingleVertexArrayOutputTypeGroup &operator=(const SingleVertexArrayOutputTypeGroup &other);
    Array::InputType m_type;
};

SingleVertexArrayOutputTypeGroup::SingleVertexArrayOutputTypeGroup(Context &context, Array::InputType type)
    : TestCaseGroup(context, Array::inputTypeToString(type).c_str(), Array::inputTypeToString(type).c_str())
    , m_type(type)
{
}

SingleVertexArrayOutputTypeGroup::~SingleVertexArrayOutputTypeGroup(void)
{
}

void SingleVertexArrayOutputTypeGroup::init(void)
{
    Array::OutputType outputTypes[] = {Array::OUTPUTTYPE_VEC2,  Array::OUTPUTTYPE_VEC3,  Array::OUTPUTTYPE_VEC4,
                                       Array::OUTPUTTYPE_IVEC2, Array::OUTPUTTYPE_IVEC3, Array::OUTPUTTYPE_IVEC4,
                                       Array::OUTPUTTYPE_UVEC2, Array::OUTPUTTYPE_UVEC3, Array::OUTPUTTYPE_UVEC4};
    Array::Storage storages[]       = {Array::STORAGE_USER};
    int counts[]                    = {1, 256};

    for (int outputTypeNdx = 0; outputTypeNdx < DE_LENGTH_OF_ARRAY(outputTypes); outputTypeNdx++)
    {
        for (int storageNdx = 0; storageNdx < DE_LENGTH_OF_ARRAY(storages); storageNdx++)
        {
            for (int componentCount = 2; componentCount < 5; componentCount++)
            {
                for (int countNdx = 0; countNdx < DE_LENGTH_OF_ARRAY(counts); countNdx++)
                {
                    std::string name = "components" + typeToString(componentCount) + "_" +
                                       Array::outputTypeToString(outputTypes[outputTypeNdx]) + "_quads" +
                                       typeToString(counts[countNdx]);

                    const bool inputIsSignedInteger = m_type == Array::INPUTTYPE_INT ||
                                                      m_type == Array::INPUTTYPE_SHORT ||
                                                      m_type == Array::INPUTTYPE_BYTE;
                    const bool inputIsUnignedInteger = m_type == Array::INPUTTYPE_UNSIGNED_INT ||
                                                       m_type == Array::INPUTTYPE_UNSIGNED_SHORT ||
                                                       m_type == Array::INPUTTYPE_UNSIGNED_BYTE;
                    const bool outputIsSignedInteger = outputTypes[outputTypeNdx] == Array::OUTPUTTYPE_IVEC2 ||
                                                       outputTypes[outputTypeNdx] == Array::OUTPUTTYPE_IVEC3 ||
                                                       outputTypes[outputTypeNdx] == Array::OUTPUTTYPE_IVEC4;
                    const bool outputIsUnsignedInteger = outputTypes[outputTypeNdx] == Array::OUTPUTTYPE_UVEC2 ||
                                                         outputTypes[outputTypeNdx] == Array::OUTPUTTYPE_UVEC3 ||
                                                         outputTypes[outputTypeNdx] == Array::OUTPUTTYPE_UVEC4;

                    // If input type is float type and output type is int type skip
                    if ((m_type == Array::INPUTTYPE_FLOAT || m_type == Array::INPUTTYPE_HALF ||
                         m_type == Array::INPUTTYPE_FIXED) &&
                        (outputTypes[outputTypeNdx] >= Array::OUTPUTTYPE_INT))
                        continue;

                    if ((m_type == Array::INPUTTYPE_UNSIGNED_INT_2_10_10_10 ||
                         m_type == Array::INPUTTYPE_INT_2_10_10_10) &&
                        (outputTypes[outputTypeNdx] >= Array::OUTPUTTYPE_INT))
                        continue;

                    if ((m_type == Array::INPUTTYPE_UNSIGNED_INT_2_10_10_10 ||
                         m_type == Array::INPUTTYPE_INT_2_10_10_10) &&
                        componentCount != 4)
                        continue;

                    // Loading signed data as unsigned causes undefined values and vice versa
                    if (inputIsSignedInteger && outputIsUnsignedInteger)
                        continue;
                    if (inputIsUnignedInteger && outputIsSignedInteger)
                        continue;

                    MultiVertexArrayTest::Spec::ArraySpec arraySpec(
                        m_type, outputTypes[outputTypeNdx], storages[storageNdx], Array::USAGE_DYNAMIC_DRAW,
                        componentCount, 0, 0, false, GLValue::getMinValue(m_type), GLValue::getMaxValue(m_type));

                    MultiVertexArrayTest::Spec spec;
                    spec.primitive = Array::PRIMITIVE_TRIANGLES;
                    spec.drawCount = counts[countNdx];
                    spec.first     = 0;
                    spec.arrays.push_back(arraySpec);

                    addChild(new MultiVertexArrayTest(m_testCtx, m_context.getRenderContext(), spec, name.c_str(),
                                                      name.c_str()));
                }
            }
        }
    }
}

class SingleVertexArrayOutputTypeTests : public TestCaseGroup
{
public:
    SingleVertexArrayOutputTypeTests(Context &context);
    virtual ~SingleVertexArrayOutputTypeTests(void);

    virtual void init(void);

private:
    SingleVertexArrayOutputTypeTests(const SingleVertexArrayOutputTypeTests &other);
    SingleVertexArrayOutputTypeTests &operator=(const SingleVertexArrayOutputTypeTests &other);
};

SingleVertexArrayOutputTypeTests::SingleVertexArrayOutputTypeTests(Context &context)
    : TestCaseGroup(context, "output_types", "Single output type vertex atribute")
{
}

SingleVertexArrayOutputTypeTests::~SingleVertexArrayOutputTypeTests(void)
{
}

void SingleVertexArrayOutputTypeTests::init(void)
{
    // Test output types with different input types, component counts and storage, Usage?, Precision?, float?
    Array::InputType inputTypes[] = {Array::INPUTTYPE_FLOAT,         Array::INPUTTYPE_SHORT,
                                     Array::INPUTTYPE_BYTE,          Array::INPUTTYPE_UNSIGNED_SHORT,
                                     Array::INPUTTYPE_UNSIGNED_BYTE, Array::INPUTTYPE_FIXED,
                                     Array::INPUTTYPE_UNSIGNED_INT,  Array::INPUTTYPE_INT,
                                     Array::INPUTTYPE_HALF,          Array::INPUTTYPE_UNSIGNED_INT_2_10_10_10,
                                     Array::INPUTTYPE_INT_2_10_10_10};

    for (int inputTypeNdx = 0; inputTypeNdx < DE_LENGTH_OF_ARRAY(inputTypes); inputTypeNdx++)
    {
        addChild(new SingleVertexArrayOutputTypeGroup(m_context, inputTypes[inputTypeNdx]));
    }
}

class SingleVertexArrayTestGroup : public TestCaseGroup
{
public:
    SingleVertexArrayTestGroup(Context &context);
    virtual ~SingleVertexArrayTestGroup(void);

    virtual void init(void);

private:
    SingleVertexArrayTestGroup(const SingleVertexArrayTestGroup &other);
    SingleVertexArrayTestGroup &operator=(const SingleVertexArrayTestGroup &other);
};

SingleVertexArrayTestGroup::SingleVertexArrayTestGroup(Context &context)
    : TestCaseGroup(context, "single_attribute", "Single vertex atribute")
{
}

SingleVertexArrayTestGroup::~SingleVertexArrayTestGroup(void)
{
}

void SingleVertexArrayTestGroup::init(void)
{
    addChild(new SingleVertexArrayStrideTests(m_context));
    addChild(new SingleVertexArrayNormalizeTests(m_context));
    addChild(new SingleVertexArrayOutputTypeTests(m_context));
    addChild(new SingleVertexArrayUsageTests(m_context));
    addChild(new SingleVertexArrayOffsetTests(m_context));
    addChild(new SingleVertexArrayFirstTests(m_context));
}

class MultiVertexArrayCountTests : public TestCaseGroup
{
public:
    MultiVertexArrayCountTests(Context &context);
    virtual ~MultiVertexArrayCountTests(void);

    virtual void init(void);

private:
    MultiVertexArrayCountTests(const MultiVertexArrayCountTests &other);
    MultiVertexArrayCountTests &operator=(const MultiVertexArrayCountTests &other);

    std::string getTestName(const MultiVertexArrayTest::Spec &spec);
};

MultiVertexArrayCountTests::MultiVertexArrayCountTests(Context &context)
    : TestCaseGroup(context, "attribute_count", "Attribute counts")
{
}

MultiVertexArrayCountTests::~MultiVertexArrayCountTests(void)
{
}

std::string MultiVertexArrayCountTests::getTestName(const MultiVertexArrayTest::Spec &spec)
{
    std::stringstream name;
    name << spec.arrays.size();

    return name.str();
}

void MultiVertexArrayCountTests::init(void)
{
    // Test attribute counts
    int arrayCounts[] = {2, 3, 4, 5, 6, 7, 8};

    for (int arrayCountNdx = 0; arrayCountNdx < DE_LENGTH_OF_ARRAY(arrayCounts); arrayCountNdx++)
    {
        MultiVertexArrayTest::Spec spec;

        spec.primitive = Array::PRIMITIVE_TRIANGLES;
        spec.drawCount = 256;
        spec.first     = 0;

        for (int arrayNdx = 0; arrayNdx < arrayCounts[arrayCountNdx]; arrayNdx++)
        {
            MultiVertexArrayTest::Spec::ArraySpec arraySpec(
                Array::INPUTTYPE_FLOAT, Array::OUTPUTTYPE_VEC2, Array::STORAGE_USER, Array::USAGE_DYNAMIC_DRAW, 2, 0, 0,
                false, GLValue::getMinValue(Array::INPUTTYPE_FLOAT), GLValue::getMaxValue(Array::INPUTTYPE_FLOAT));

            spec.arrays.push_back(arraySpec);
        }

        std::string name = getTestName(spec);
        std::string desc = getTestName(spec);

        addChild(new MultiVertexArrayTest(m_testCtx, m_context.getRenderContext(), spec, name.c_str(), desc.c_str()));
    }
}

class MultiVertexArrayStorageTests : public TestCaseGroup
{
public:
    MultiVertexArrayStorageTests(Context &context);
    virtual ~MultiVertexArrayStorageTests(void);

    virtual void init(void);

private:
    MultiVertexArrayStorageTests(const MultiVertexArrayStorageTests &other);
    MultiVertexArrayStorageTests &operator=(const MultiVertexArrayStorageTests &other);

    void addStorageCases(MultiVertexArrayTest::Spec spec, int depth);
    std::string getTestName(const MultiVertexArrayTest::Spec &spec);
};

MultiVertexArrayStorageTests::MultiVertexArrayStorageTests(Context &context)
    : TestCaseGroup(context, "storage", "Attribute storages")
{
}

MultiVertexArrayStorageTests::~MultiVertexArrayStorageTests(void)
{
}

std::string MultiVertexArrayStorageTests::getTestName(const MultiVertexArrayTest::Spec &spec)
{
    std::stringstream name;
    name << spec.arrays.size();

    for (int arrayNdx = 0; arrayNdx < (int)spec.arrays.size(); arrayNdx++)
    {
        name << "_" << Array::storageToString(spec.arrays[arrayNdx].storage);
    }

    return name.str();
}

void MultiVertexArrayStorageTests::addStorageCases(MultiVertexArrayTest::Spec spec, int depth)
{
    if (depth == 0)
    {
        // Skip trivial case, used elsewhere
        bool ok = false;
        for (int arrayNdx = 0; arrayNdx < (int)spec.arrays.size(); arrayNdx++)
        {
            if (spec.arrays[arrayNdx].storage != Array::STORAGE_USER)
            {
                ok = true;
                break;
            }
        }

        if (!ok)
            return;

        std::string name = getTestName(spec);
        std::string desc = getTestName(spec);

        addChild(new MultiVertexArrayTest(m_testCtx, m_context.getRenderContext(), spec, name.c_str(), desc.c_str()));
        return;
    }

    Array::Storage storages[] = {Array::STORAGE_USER, Array::STORAGE_BUFFER};
    for (int storageNdx = 0; storageNdx < DE_LENGTH_OF_ARRAY(storages); storageNdx++)
    {
        MultiVertexArrayTest::Spec::ArraySpec arraySpec(
            Array::INPUTTYPE_FLOAT, Array::OUTPUTTYPE_VEC2, storages[storageNdx], Array::USAGE_DYNAMIC_DRAW, 2, 0, 0,
            false, GLValue::getMinValue(Array::INPUTTYPE_FLOAT), GLValue::getMaxValue(Array::INPUTTYPE_FLOAT));

        MultiVertexArrayTest::Spec _spec = spec;
        _spec.arrays.push_back(arraySpec);
        addStorageCases(_spec, depth - 1);
    }
}

void MultiVertexArrayStorageTests::init(void)
{
    // Test different storages
    int arrayCounts[] = {3};

    MultiVertexArrayTest::Spec spec;

    spec.primitive = Array::PRIMITIVE_TRIANGLES;
    spec.drawCount = 256;
    spec.first     = 0;

    for (int arrayCountNdx = 0; arrayCountNdx < DE_LENGTH_OF_ARRAY(arrayCounts); arrayCountNdx++)
        addStorageCases(spec, arrayCounts[arrayCountNdx]);
}

class MultiVertexArrayStrideTests : public TestCaseGroup
{
public:
    MultiVertexArrayStrideTests(Context &context);
    virtual ~MultiVertexArrayStrideTests(void);

    virtual void init(void);

private:
    MultiVertexArrayStrideTests(const MultiVertexArrayStrideTests &other);
    MultiVertexArrayStrideTests &operator=(const MultiVertexArrayStrideTests &other);

    void addStrideCases(MultiVertexArrayTest::Spec spec, int depth);
    std::string getTestName(const MultiVertexArrayTest::Spec &spec);
};

MultiVertexArrayStrideTests::MultiVertexArrayStrideTests(Context &context) : TestCaseGroup(context, "stride", "Strides")
{
}

MultiVertexArrayStrideTests::~MultiVertexArrayStrideTests(void)
{
}

std::string MultiVertexArrayStrideTests::getTestName(const MultiVertexArrayTest::Spec &spec)
{
    std::stringstream name;

    name << spec.arrays.size();

    for (int arrayNdx = 0; arrayNdx < (int)spec.arrays.size(); arrayNdx++)
    {
        name << "_" << Array::inputTypeToString(spec.arrays[arrayNdx].inputType) << spec.arrays[arrayNdx].componentCount
             << "_" << spec.arrays[arrayNdx].stride;
    }

    return name.str();
}

void MultiVertexArrayStrideTests::init(void)
{
    // Test different strides, with multiple arrays, input types??
    int arrayCounts[] = {3};

    MultiVertexArrayTest::Spec spec;

    spec.primitive = Array::PRIMITIVE_TRIANGLES;
    spec.drawCount = 256;
    spec.first     = 0;

    for (int arrayCountNdx = 0; arrayCountNdx < DE_LENGTH_OF_ARRAY(arrayCounts); arrayCountNdx++)
        addStrideCases(spec, arrayCounts[arrayCountNdx]);
}

void MultiVertexArrayStrideTests::addStrideCases(MultiVertexArrayTest::Spec spec, int depth)
{
    if (depth == 0)
    {
        std::string name = getTestName(spec);
        std::string desc = getTestName(spec);
        addChild(new MultiVertexArrayTest(m_testCtx, m_context.getRenderContext(), spec, name.c_str(), desc.c_str()));
        return;
    }

    int strides[] = {0, -1, 17, 32};

    for (int strideNdx = 0; strideNdx < DE_LENGTH_OF_ARRAY(strides); strideNdx++)
    {
        const int componentCount = 2;
        MultiVertexArrayTest::Spec::ArraySpec arraySpec(
            Array::INPUTTYPE_FLOAT, Array::OUTPUTTYPE_VEC2, Array::STORAGE_USER, Array::USAGE_DYNAMIC_DRAW,
            componentCount, 0,
            (strides[strideNdx] >= 0 ? strides[strideNdx] :
                                       componentCount * Array::inputTypeSize(Array::INPUTTYPE_FLOAT)),
            false, GLValue::getMinValue(Array::INPUTTYPE_FLOAT), GLValue::getMaxValue(Array::INPUTTYPE_FLOAT));

        MultiVertexArrayTest::Spec _spec = spec;
        _spec.arrays.push_back(arraySpec);
        addStrideCases(_spec, depth - 1);
    }
}

class MultiVertexArrayOutputTests : public TestCaseGroup
{
public:
    MultiVertexArrayOutputTests(Context &context);
    virtual ~MultiVertexArrayOutputTests(void);

    virtual void init(void);

private:
    MultiVertexArrayOutputTests(const MultiVertexArrayOutputTests &other);
    MultiVertexArrayOutputTests &operator=(const MultiVertexArrayOutputTests &other);

    void addInputTypeCases(MultiVertexArrayTest::Spec spec, int depth);
    std::string getTestName(const MultiVertexArrayTest::Spec &spec);
};

MultiVertexArrayOutputTests::MultiVertexArrayOutputTests(Context &context)
    : TestCaseGroup(context, "input_types", "input types")
{
}

MultiVertexArrayOutputTests::~MultiVertexArrayOutputTests(void)
{
}

std::string MultiVertexArrayOutputTests::getTestName(const MultiVertexArrayTest::Spec &spec)
{
    std::stringstream name;

    name << spec.arrays.size();

    for (int arrayNdx = 0; arrayNdx < (int)spec.arrays.size(); arrayNdx++)
    {
        name << "_" << Array::inputTypeToString(spec.arrays[arrayNdx].inputType) << spec.arrays[arrayNdx].componentCount
             << "_" << Array::outputTypeToString(spec.arrays[arrayNdx].outputType);
    }

    return name.str();
}

void MultiVertexArrayOutputTests::init(void)
{
    // Test different input types, with multiple arrays
    int arrayCounts[] = {3};

    MultiVertexArrayTest::Spec spec;

    spec.primitive = Array::PRIMITIVE_TRIANGLES;
    spec.drawCount = 256;
    spec.first     = 0;

    for (int arrayCountNdx = 0; arrayCountNdx < DE_LENGTH_OF_ARRAY(arrayCounts); arrayCountNdx++)
        addInputTypeCases(spec, arrayCounts[arrayCountNdx]);
}

void MultiVertexArrayOutputTests::addInputTypeCases(MultiVertexArrayTest::Spec spec, int depth)
{
    if (depth == 0)
    {
        std::string name = getTestName(spec);
        std::string desc = getTestName(spec);
        addChild(new MultiVertexArrayTest(m_testCtx, m_context.getRenderContext(), spec, name.c_str(), desc.c_str()));
        return;
    }

    Array::InputType inputTypes[] = {Array::INPUTTYPE_FIXED, Array::INPUTTYPE_BYTE, Array::INPUTTYPE_SHORT,
                                     Array::INPUTTYPE_UNSIGNED_BYTE, Array::INPUTTYPE_UNSIGNED_SHORT};
    for (int inputTypeNdx = 0; inputTypeNdx < DE_LENGTH_OF_ARRAY(inputTypes); inputTypeNdx++)
    {
        MultiVertexArrayTest::Spec::ArraySpec arraySpec(
            inputTypes[inputTypeNdx], Array::OUTPUTTYPE_VEC2, Array::STORAGE_USER, Array::USAGE_DYNAMIC_DRAW, 2, 0, 0,
            false, GLValue::getMinValue(inputTypes[inputTypeNdx]), GLValue::getMaxValue(inputTypes[inputTypeNdx]));

        MultiVertexArrayTest::Spec _spec = spec;
        _spec.arrays.push_back(arraySpec);
        addInputTypeCases(_spec, depth - 1);
    }
}

class MultiVertexArrayTestGroup : public TestCaseGroup
{
public:
    MultiVertexArrayTestGroup(Context &context);
    virtual ~MultiVertexArrayTestGroup(void);

    virtual void init(void);

private:
    MultiVertexArrayTestGroup(const MultiVertexArrayTestGroup &other);
    MultiVertexArrayTestGroup &operator=(const MultiVertexArrayTestGroup &other);
};

MultiVertexArrayTestGroup::MultiVertexArrayTestGroup(Context &context)
    : TestCaseGroup(context, "multiple_attributes", "Multiple vertex atributes")
{
}

MultiVertexArrayTestGroup::~MultiVertexArrayTestGroup(void)
{
}

void MultiVertexArrayTestGroup::init(void)
{
    addChild(new MultiVertexArrayCountTests(m_context));
    addChild(new MultiVertexArrayStorageTests(m_context));
    addChild(new MultiVertexArrayStrideTests(m_context));
    addChild(new MultiVertexArrayOutputTests(m_context));
}

VertexArrayTestGroup::VertexArrayTestGroup(Context &context)
    : TestCaseGroup(context, "vertex_arrays", "Vertex array and array tests")
{
}

VertexArrayTestGroup::~VertexArrayTestGroup(void)
{
}

void VertexArrayTestGroup::init(void)
{
    addChild(new SingleVertexArrayTestGroup(m_context));
    addChild(new MultiVertexArrayTestGroup(m_context));
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
