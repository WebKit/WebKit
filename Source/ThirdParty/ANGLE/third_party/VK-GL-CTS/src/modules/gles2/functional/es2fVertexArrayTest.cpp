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
 * \brief Vertex array and buffer tests
 *//*--------------------------------------------------------------------*/

#include "es2fVertexArrayTest.hpp"
#include "glsVertexArrayTests.hpp"

#include "glwEnums.hpp"

using namespace deqp::gls;

namespace deqp
{
namespace gles2
{
namespace Functional
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
    Array::Usage usages[] = {Array::USAGE_STATIC_DRAW, Array::USAGE_STREAM_DRAW, Array::USAGE_DYNAMIC_DRAW};
    int counts[]          = {1, 256};
    int strides[]         = {0, -1, 17, 32}; // Tread negative value as sizeof input. Same as 0, but done outside of GL.
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

                    if (aligned)
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
    Array::Storage storages[] = {Array::STORAGE_USER, Array::STORAGE_BUFFER};
    int counts[]              = {1, 256};
    int strides[] = {/*0,*/ -1, 17, 32}; // Tread negative value as sizeof input. Same as 0, but done outside of GL.

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
                        const bool bufferAligned = (storages[storageNdx] == Array::STORAGE_BUFFER) &&
                                                   (stride % Array::inputTypeSize(inputTypes[inputTypeNdx])) == 0;

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
                        if (bufferAligned)
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
    int offsets[]                 = {1, 16, 17};
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
                        if (aligned)
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
                    if (aligned)
                        addChild(new MultiVertexArrayTest(m_testCtx, m_context.getRenderContext(), spec, name.c_str(),
                                                          name.c_str()));
                }
            }
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
                                     Array::INPUTTYPE_UNSIGNED_BYTE, Array::INPUTTYPE_FIXED};
    Array::Storage storages[]     = {Array::STORAGE_USER};
    int counts[]                  = {1, 256};

    for (int inputTypeNdx = 0; inputTypeNdx < DE_LENGTH_OF_ARRAY(inputTypes); inputTypeNdx++)
    {
        for (int storageNdx = 0; storageNdx < DE_LENGTH_OF_ARRAY(storages); storageNdx++)
        {
            for (int componentCount = 2; componentCount < 5; componentCount++)
            {
                for (int countNdx = 0; countNdx < DE_LENGTH_OF_ARRAY(counts); countNdx++)
                {
                    MultiVertexArrayTest::Spec::ArraySpec arraySpec(
                        inputTypes[inputTypeNdx], Array::OUTPUTTYPE_VEC4, storages[storageNdx],
                        Array::USAGE_DYNAMIC_DRAW, componentCount, 0, 0, true,
                        GLValue::getMinValue(inputTypes[inputTypeNdx]), GLValue::getMaxValue(inputTypes[inputTypeNdx]));

                    MultiVertexArrayTest::Spec spec;
                    spec.primitive = Array::PRIMITIVE_TRIANGLES;
                    spec.drawCount = counts[countNdx];
                    spec.first     = 0;
                    spec.arrays.push_back(arraySpec);

                    std::string name = spec.getName();
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
    Array::InputType inputTypes[]   = {Array::INPUTTYPE_FLOAT,         Array::INPUTTYPE_SHORT,
                                       Array::INPUTTYPE_BYTE,          Array::INPUTTYPE_UNSIGNED_SHORT,
                                       Array::INPUTTYPE_UNSIGNED_BYTE, Array::INPUTTYPE_FIXED};
    Array::OutputType outputTypes[] = {Array::OUTPUTTYPE_VEC2, Array::OUTPUTTYPE_VEC3, Array::OUTPUTTYPE_VEC4};
    Array::Storage storages[]       = {Array::STORAGE_USER};
    int counts[]                    = {1, 256};

    for (int inputTypeNdx = 0; inputTypeNdx < DE_LENGTH_OF_ARRAY(inputTypes); inputTypeNdx++)
    {
        for (int outputTypeNdx = 0; outputTypeNdx < DE_LENGTH_OF_ARRAY(outputTypes); outputTypeNdx++)
        {
            for (int storageNdx = 0; storageNdx < DE_LENGTH_OF_ARRAY(storages); storageNdx++)
            {
                for (int componentCount = 2; componentCount < 5; componentCount++)
                {
                    for (int countNdx = 0; countNdx < DE_LENGTH_OF_ARRAY(counts); countNdx++)
                    {
                        MultiVertexArrayTest::Spec::ArraySpec arraySpec(
                            inputTypes[inputTypeNdx], outputTypes[outputTypeNdx], storages[storageNdx],
                            Array::USAGE_DYNAMIC_DRAW, componentCount, 0, 0, false,
                            GLValue::getMinValue(inputTypes[inputTypeNdx]),
                            GLValue::getMaxValue(inputTypes[inputTypeNdx]));

                        MultiVertexArrayTest::Spec spec;
                        spec.primitive = Array::PRIMITIVE_TRIANGLES;
                        spec.drawCount = counts[countNdx];
                        spec.first     = 0;
                        spec.arrays.push_back(arraySpec);

                        std::string name = spec.getName();
                        addChild(new MultiVertexArrayTest(m_testCtx, m_context.getRenderContext(), spec, name.c_str(),
                                                          name.c_str()));
                    }
                }
            }
        }
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
} // namespace gles2
} // namespace deqp
