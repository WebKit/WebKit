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
 * \brief Vertex array and buffer unaligned access stress tests
 *//*--------------------------------------------------------------------*/

#include "es2sVertexArrayTests.hpp"
#include "glsVertexArrayTests.hpp"

#include "glwEnums.hpp"

using namespace deqp::gls;

namespace deqp
{
namespace gles2
{
namespace Stress
{
namespace
{

template <class T>
static std::string typeToString(T t)
{
    std::stringstream strm;
    strm << t;
    return strm.str();
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
    Array::Usage usages[]         = {Array::USAGE_STATIC_DRAW, Array::USAGE_STREAM_DRAW, Array::USAGE_DYNAMIC_DRAW};
    int counts[]                  = {1, 256};
    int strides[]                 = {17};
    Array::InputType inputTypes[] = {Array::INPUTTYPE_FLOAT, Array::INPUTTYPE_FIXED, Array::INPUTTYPE_SHORT,
                                     Array::INPUTTYPE_BYTE};

    for (int inputTypeNdx = 0; inputTypeNdx < DE_LENGTH_OF_ARRAY(inputTypes); inputTypeNdx++)
    {
        for (int countNdx = 0; countNdx < DE_LENGTH_OF_ARRAY(counts); countNdx++)
        {
            for (int strideNdx = 0; strideNdx < DE_LENGTH_OF_ARRAY(strides); strideNdx++)
            {
                for (int usageNdx = 0; usageNdx < DE_LENGTH_OF_ARRAY(usages); usageNdx++)
                {
                    const int componentCount = 2;
                    const int stride =
                        (strides[strideNdx] < 0 ? Array::inputTypeSize(inputTypes[inputTypeNdx]) * componentCount :
                                                  strides[strideNdx]);
                    const bool aligned = (stride % Array::inputTypeSize(inputTypes[inputTypeNdx])) == 0;
                    MultiVertexArrayTest::Spec::ArraySpec arraySpec(
                        inputTypes[inputTypeNdx], Array::OUTPUTTYPE_VEC2, Array::STORAGE_BUFFER, usages[usageNdx],
                        componentCount, 0, stride, false, GLValue::getMinValue(inputTypes[inputTypeNdx]),
                        GLValue::getMaxValue(inputTypes[inputTypeNdx]));

                    MultiVertexArrayTest::Spec spec;
                    spec.primitive = Array::PRIMITIVE_TRIANGLES;
                    spec.drawCount = counts[countNdx];
                    spec.first     = 0;
                    spec.arrays.push_back(arraySpec);

                    std::string name = spec.getName();

                    if (!aligned)
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
    // Test strides with different input types, component counts and storage, Usage(?)
    Array::InputType inputTypes[] = {
        Array::INPUTTYPE_FLOAT, Array::INPUTTYPE_SHORT, Array::INPUTTYPE_BYTE,
        /*Array::INPUTTYPE_UNSIGNED_SHORT, Array::INPUTTYPE_UNSIGNED_BYTE,*/ Array::INPUTTYPE_FIXED};
    Array::Storage storages[] = {Array::STORAGE_BUFFER};
    int counts[]              = {1, 256};
    int strides[]             = {17};

    for (int inputTypeNdx = 0; inputTypeNdx < DE_LENGTH_OF_ARRAY(inputTypes); inputTypeNdx++)
    {
        for (int storageNdx = 0; storageNdx < DE_LENGTH_OF_ARRAY(storages); storageNdx++)
        {
            for (int componentCount = 2; componentCount < 5; componentCount++)
            {
                for (int countNdx = 0; countNdx < DE_LENGTH_OF_ARRAY(counts); countNdx++)
                {
                    for (int strideNdx = 0; strideNdx < DE_LENGTH_OF_ARRAY(strides); strideNdx++)
                    {
                        const int stride =
                            (strides[strideNdx] < 0 ? Array::inputTypeSize(inputTypes[inputTypeNdx]) * componentCount :
                                                      strides[strideNdx]);
                        const bool bufferUnaligned = (storages[storageNdx] == Array::STORAGE_BUFFER) &&
                                                     (stride % Array::inputTypeSize(inputTypes[inputTypeNdx])) != 0;

                        MultiVertexArrayTest::Spec::ArraySpec arraySpec(
                            inputTypes[inputTypeNdx], Array::OUTPUTTYPE_VEC4, storages[storageNdx],
                            Array::USAGE_DYNAMIC_DRAW, componentCount, 0, stride, false,
                            GLValue::getMinValue(inputTypes[inputTypeNdx]),
                            GLValue::getMaxValue(inputTypes[inputTypeNdx]));

                        MultiVertexArrayTest::Spec spec;
                        spec.primitive = Array::PRIMITIVE_TRIANGLES;
                        spec.drawCount = counts[countNdx];
                        spec.first     = 0;
                        spec.arrays.push_back(arraySpec);

                        std::string name = spec.getName();
                        if (bufferUnaligned)
                            addChild(new MultiVertexArrayTest(m_testCtx, m_context.getRenderContext(), spec,
                                                              name.c_str(), name.c_str()));
                    }
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
    : TestCaseGroup(context, "first", "Single vertex atribute different first values")
{
}

SingleVertexArrayFirstTests::~SingleVertexArrayFirstTests(void)
{
}

void SingleVertexArrayFirstTests::init(void)
{
    // Test strides with different input types, component counts and storage, Usage(?)
    Array::InputType inputTypes[] = {Array::INPUTTYPE_FLOAT, Array::INPUTTYPE_BYTE, Array::INPUTTYPE_FIXED};
    int counts[]                  = {5, 256};
    int firsts[]                  = {6, 24};
    int offsets[]                 = {1, 17};
    int strides[] = {/*0,*/ -1, 17, 32}; // Tread negative value as sizeof input. Same as 0, but done outside of GL.

    for (int inputTypeNdx = 0; inputTypeNdx < DE_LENGTH_OF_ARRAY(inputTypes); inputTypeNdx++)
    {
        for (int offsetNdx = 0; offsetNdx < DE_LENGTH_OF_ARRAY(offsets); offsetNdx++)
        {
            for (int countNdx = 0; countNdx < DE_LENGTH_OF_ARRAY(counts); countNdx++)
            {
                for (int strideNdx = 0; strideNdx < DE_LENGTH_OF_ARRAY(strides); strideNdx++)
                {
                    for (int firstNdx = 0; firstNdx < DE_LENGTH_OF_ARRAY(firsts); firstNdx++)
                    {
                        const int stride =
                            (strides[strideNdx] < 0 ? Array::inputTypeSize(inputTypes[inputTypeNdx]) * 2 :
                                                      strides[strideNdx]);
                        const bool aligned = ((stride % Array::inputTypeSize(inputTypes[inputTypeNdx])) == 0) &&
                                             (offsets[offsetNdx] % Array::inputTypeSize(inputTypes[inputTypeNdx]) == 0);

                        MultiVertexArrayTest::Spec::ArraySpec arraySpec(
                            inputTypes[inputTypeNdx], Array::OUTPUTTYPE_VEC2, Array::STORAGE_BUFFER,
                            Array::USAGE_DYNAMIC_DRAW, 2, offsets[offsetNdx], stride, false,
                            GLValue::getMinValue(inputTypes[inputTypeNdx]),
                            GLValue::getMaxValue(inputTypes[inputTypeNdx]));

                        MultiVertexArrayTest::Spec spec;
                        spec.primitive = Array::PRIMITIVE_TRIANGLES;
                        spec.drawCount = counts[countNdx];
                        spec.first     = firsts[firstNdx];
                        spec.arrays.push_back(arraySpec);

                        std::string name = Array::inputTypeToString(inputTypes[inputTypeNdx]) + "_first" +
                                           typeToString(firsts[firstNdx]) + "_offset" +
                                           typeToString(offsets[offsetNdx]) + "_stride" + typeToString(stride) +
                                           "_quads" + typeToString(counts[countNdx]);
                        if (!aligned)
                            addChild(new MultiVertexArrayTest(m_testCtx, m_context.getRenderContext(), spec,
                                                              name.c_str(), name.c_str()));
                    }
                }
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
    // Test strides with different input types, component counts and storage, Usage(?)
    Array::InputType inputTypes[] = {Array::INPUTTYPE_FLOAT, Array::INPUTTYPE_BYTE, Array::INPUTTYPE_FIXED};
    int counts[]                  = {1, 256};
    int offsets[]                 = {1, 4, 17, 32};
    int strides[] = {/*0,*/ -1, 17, 32}; // Tread negative value as sizeof input. Same as 0, but done outside of GL.

    for (int inputTypeNdx = 0; inputTypeNdx < DE_LENGTH_OF_ARRAY(inputTypes); inputTypeNdx++)
    {
        for (int offsetNdx = 0; offsetNdx < DE_LENGTH_OF_ARRAY(offsets); offsetNdx++)
        {
            for (int countNdx = 0; countNdx < DE_LENGTH_OF_ARRAY(counts); countNdx++)
            {
                for (int strideNdx = 0; strideNdx < DE_LENGTH_OF_ARRAY(strides); strideNdx++)
                {
                    const int stride   = (strides[strideNdx] < 0 ? Array::inputTypeSize(inputTypes[inputTypeNdx]) * 2 :
                                                                   strides[strideNdx]);
                    const bool aligned = ((stride % Array::inputTypeSize(inputTypes[inputTypeNdx])) == 0) &&
                                         ((offsets[offsetNdx] % Array::inputTypeSize(inputTypes[inputTypeNdx])) == 0);

                    MultiVertexArrayTest::Spec::ArraySpec arraySpec(
                        inputTypes[inputTypeNdx], Array::OUTPUTTYPE_VEC2, Array::STORAGE_BUFFER,
                        Array::USAGE_DYNAMIC_DRAW, 2, offsets[offsetNdx], stride, false,
                        GLValue::getMinValue(inputTypes[inputTypeNdx]), GLValue::getMaxValue(inputTypes[inputTypeNdx]));

                    MultiVertexArrayTest::Spec spec;
                    spec.primitive = Array::PRIMITIVE_TRIANGLES;
                    spec.drawCount = counts[countNdx];
                    spec.first     = 0;
                    spec.arrays.push_back(arraySpec);

                    std::string name = spec.getName();
                    if (!aligned)
                        addChild(new MultiVertexArrayTest(m_testCtx, m_context.getRenderContext(), spec, name.c_str(),
                                                          name.c_str()));
                }
            }
        }
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
} // namespace gles2
} // namespace deqp
