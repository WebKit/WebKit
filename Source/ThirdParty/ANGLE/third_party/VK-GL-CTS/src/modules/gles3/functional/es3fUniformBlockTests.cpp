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
 * \brief Uniform block tests.
 *//*--------------------------------------------------------------------*/

#include "es3fUniformBlockTests.hpp"
#include "glsUniformBlockCase.hpp"
#include "glsRandomUniformBlockCase.hpp"
#include "tcuCommandLine.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"

using std::string;
using std::vector;

namespace deqp
{
namespace gles3
{
namespace Functional
{

using gls::RandomUniformBlockCase;
using gls::UniformBlockCase;
using namespace gls::ub;

static void createRandomCaseGroup(tcu::TestCaseGroup *parentGroup, Context &context, const char *groupName,
                                  const char *description, UniformBlockCase::BufferMode bufferMode, uint32_t features,
                                  int numCases, uint32_t baseSeed)
{
    tcu::TestCaseGroup *group = new tcu::TestCaseGroup(context.getTestContext(), groupName, description);
    parentGroup->addChild(group);

    baseSeed += (uint32_t)context.getTestContext().getCommandLine().getBaseSeed();

    for (int ndx = 0; ndx < numCases; ndx++)
        group->addChild(new RandomUniformBlockCase(context.getTestContext(), context.getRenderContext(),
                                                   glu::GLSL_VERSION_300_ES, de::toString(ndx).c_str(), "", bufferMode,
                                                   features, (uint32_t)ndx + baseSeed));
}

class BlockBasicTypeCase : public UniformBlockCase
{
public:
    BlockBasicTypeCase(Context &context, const char *name, const char *description, const VarType &type,
                       uint32_t layoutFlags, int numInstances)
        : UniformBlockCase(context.getTestContext(), context.getRenderContext(), name, description,
                           glu::GLSL_VERSION_300_ES, BUFFERMODE_PER_BLOCK)
    {
        UniformBlock &block = m_interface.allocBlock("Block");
        block.addUniform(Uniform("var", type, 0));
        block.setFlags(layoutFlags);

        if (numInstances > 0)
        {
            block.setArraySize(numInstances);
            block.setInstanceName("block");
        }
    }
};

static void createBlockBasicTypeCases(tcu::TestCaseGroup *group, Context &context, const char *name,
                                      const VarType &type, uint32_t layoutFlags, int numInstances = 0)
{
    group->addChild(new BlockBasicTypeCase(context, (string(name) + "_vertex").c_str(), "", type,
                                           layoutFlags | DECLARE_VERTEX, numInstances));
    group->addChild(new BlockBasicTypeCase(context, (string(name) + "_fragment").c_str(), "", type,
                                           layoutFlags | DECLARE_FRAGMENT, numInstances));

    if (!(layoutFlags & LAYOUT_PACKED))
        group->addChild(new BlockBasicTypeCase(context, (string(name) + "_both").c_str(), "", type,
                                               layoutFlags | DECLARE_VERTEX | DECLARE_FRAGMENT, numInstances));
}

class BlockSingleStructCase : public UniformBlockCase
{
public:
    BlockSingleStructCase(Context &context, const char *name, const char *description, uint32_t layoutFlags,
                          BufferMode bufferMode, int numInstances)
        : UniformBlockCase(context.getTestContext(), context.getRenderContext(), name, description,
                           glu::GLSL_VERSION_300_ES, bufferMode)
        , m_layoutFlags(layoutFlags)
        , m_numInstances(numInstances)
    {
    }

    void init(void)
    {
        StructType &typeS = m_interface.allocStruct("S");
        typeS.addMember("a", VarType(glu::TYPE_INT_VEC3, PRECISION_HIGH), UNUSED_BOTH); // First member is unused.
        typeS.addMember("b", VarType(VarType(glu::TYPE_FLOAT_MAT3, PRECISION_MEDIUM), 4));
        typeS.addMember("c", VarType(glu::TYPE_FLOAT_VEC4, PRECISION_HIGH));

        UniformBlock &block = m_interface.allocBlock("Block");
        block.addUniform(Uniform("s", VarType(&typeS), 0));
        block.setFlags(m_layoutFlags);

        if (m_numInstances > 0)
        {
            block.setInstanceName("block");
            block.setArraySize(m_numInstances);
        }
    }

private:
    uint32_t m_layoutFlags;
    int m_numInstances;
};

class BlockSingleStructArrayCase : public UniformBlockCase
{
public:
    BlockSingleStructArrayCase(Context &context, const char *name, const char *description, uint32_t layoutFlags,
                               BufferMode bufferMode, int numInstances)
        : UniformBlockCase(context.getTestContext(), context.getRenderContext(), name, description,
                           glu::GLSL_VERSION_300_ES, bufferMode)
        , m_layoutFlags(layoutFlags)
        , m_numInstances(numInstances)
    {
    }

    void init(void)
    {
        StructType &typeS = m_interface.allocStruct("S");
        typeS.addMember("a", VarType(glu::TYPE_INT_VEC3, PRECISION_HIGH), UNUSED_BOTH);
        typeS.addMember("b", VarType(VarType(glu::TYPE_FLOAT_MAT3, PRECISION_MEDIUM), 4));
        typeS.addMember("c", VarType(glu::TYPE_FLOAT_VEC4, PRECISION_HIGH));

        UniformBlock &block = m_interface.allocBlock("Block");
        block.addUniform(Uniform("u", VarType(glu::TYPE_UINT, PRECISION_LOW)));
        block.addUniform(Uniform("s", VarType(VarType(&typeS), 3)));
        block.addUniform(Uniform("v", VarType(glu::TYPE_FLOAT_VEC4, PRECISION_MEDIUM)));
        block.setFlags(m_layoutFlags);

        if (m_numInstances > 0)
        {
            block.setInstanceName("block");
            block.setArraySize(m_numInstances);
        }
    }

private:
    uint32_t m_layoutFlags;
    int m_numInstances;
};

class BlockSingleNestedStructCase : public UniformBlockCase
{
public:
    BlockSingleNestedStructCase(Context &context, const char *name, const char *description, uint32_t layoutFlags,
                                BufferMode bufferMode, int numInstances)
        : UniformBlockCase(context.getTestContext(), context.getRenderContext(), name, description,
                           glu::GLSL_VERSION_300_ES, bufferMode)
        , m_layoutFlags(layoutFlags)
        , m_numInstances(numInstances)
    {
    }

    void init(void)
    {
        StructType &typeS = m_interface.allocStruct("S");
        typeS.addMember("a", VarType(glu::TYPE_INT_VEC3, PRECISION_HIGH));
        typeS.addMember("b", VarType(VarType(glu::TYPE_FLOAT_MAT3, PRECISION_MEDIUM), 4));
        typeS.addMember("c", VarType(glu::TYPE_FLOAT_VEC4, PRECISION_HIGH), UNUSED_BOTH);

        StructType &typeT = m_interface.allocStruct("T");
        typeT.addMember("a", VarType(glu::TYPE_FLOAT_MAT3, PRECISION_MEDIUM));
        typeT.addMember("b", VarType(&typeS));

        UniformBlock &block = m_interface.allocBlock("Block");
        block.addUniform(Uniform("s", VarType(&typeS), 0));
        block.addUniform(Uniform("v", VarType(glu::TYPE_FLOAT_VEC2, PRECISION_LOW), UNUSED_BOTH));
        block.addUniform(Uniform("t", VarType(&typeT), 0));
        block.addUniform(Uniform("u", VarType(glu::TYPE_UINT, PRECISION_HIGH), 0));
        block.setFlags(m_layoutFlags);

        if (m_numInstances > 0)
        {
            block.setInstanceName("block");
            block.setArraySize(m_numInstances);
        }
    }

private:
    uint32_t m_layoutFlags;
    int m_numInstances;
};

class BlockSingleNestedStructMixedMatrixPackingCase : public UniformBlockCase
{
public:
    BlockSingleNestedStructMixedMatrixPackingCase(Context &context, const char *name, const char *description,
                                                  uint32_t blockLayoutFlags, uint32_t matrixLayoutFlags,
                                                  uint32_t matrixArrayLayoutFlags, BufferMode bufferMode,
                                                  int numInstances)
        : UniformBlockCase(context.getTestContext(), context.getRenderContext(), name, description,
                           glu::GLSL_VERSION_300_ES, bufferMode)
        , m_blockLayoutFlags(blockLayoutFlags)
        , m_matrixLayoutFlags(matrixLayoutFlags)
        , m_matrixArrayLayoutFlags(matrixArrayLayoutFlags)
        , m_numInstances(numInstances)
    {
    }

    void init(void)
    {
        StructType &typeS = m_interface.allocStruct("S");
        typeS.addMember("a", VarType(glu::TYPE_INT_VEC3, PRECISION_HIGH));
        typeS.addMember("b", VarType(VarType(glu::TYPE_FLOAT_MAT3, PRECISION_MEDIUM), 4));
        typeS.addMember("c", VarType(glu::TYPE_FLOAT_VEC4, PRECISION_HIGH), UNUSED_BOTH);

        StructType &typeT = m_interface.allocStruct("T");
        typeT.addMember("a", VarType(glu::TYPE_FLOAT_MAT3, PRECISION_MEDIUM));
        typeT.addMember("b", VarType(&typeS));

        UniformBlock &block = m_interface.allocBlock("Block");
        block.addUniform(Uniform("s", VarType(&typeS, m_matrixArrayLayoutFlags), 0));
        block.addUniform(Uniform("v", VarType(glu::TYPE_FLOAT_VEC2, PRECISION_LOW), UNUSED_BOTH));
        block.addUniform(Uniform("t", VarType(&typeT, m_matrixLayoutFlags), 0));
        block.addUniform(Uniform("u", VarType(glu::TYPE_UINT, PRECISION_HIGH), 0));
        block.setFlags(m_blockLayoutFlags);

        if (m_numInstances > 0)
        {
            block.setInstanceName("block");
            block.setArraySize(m_numInstances);
        }
    }

private:
    uint32_t m_blockLayoutFlags;
    uint32_t m_matrixLayoutFlags;
    uint32_t m_matrixArrayLayoutFlags;
    int m_numInstances;
};

class BlockSingleNestedStructArrayCase : public UniformBlockCase
{
public:
    BlockSingleNestedStructArrayCase(Context &context, const char *name, const char *description, uint32_t layoutFlags,
                                     BufferMode bufferMode, int numInstances)
        : UniformBlockCase(context.getTestContext(), context.getRenderContext(), name, description,
                           glu::GLSL_VERSION_300_ES, bufferMode)
        , m_layoutFlags(layoutFlags)
        , m_numInstances(numInstances)
    {
    }

    void init(void)
    {
        StructType &typeS = m_interface.allocStruct("S");
        typeS.addMember("a", VarType(glu::TYPE_INT_VEC3, PRECISION_HIGH));
        typeS.addMember("b", VarType(VarType(glu::TYPE_INT_VEC2, PRECISION_MEDIUM), 4));
        typeS.addMember("c", VarType(glu::TYPE_FLOAT_VEC4, PRECISION_HIGH), UNUSED_BOTH);

        StructType &typeT = m_interface.allocStruct("T");
        typeT.addMember("a", VarType(glu::TYPE_FLOAT_MAT3, PRECISION_MEDIUM));
        typeT.addMember("b", VarType(VarType(&typeS), 3));

        UniformBlock &block = m_interface.allocBlock("Block");
        block.addUniform(Uniform("s", VarType(&typeS), 0));
        block.addUniform(Uniform("v", VarType(glu::TYPE_FLOAT_VEC2, PRECISION_LOW), UNUSED_BOTH));
        block.addUniform(Uniform("t", VarType(VarType(&typeT), 2), 0));
        block.addUniform(Uniform("u", VarType(glu::TYPE_UINT, PRECISION_HIGH), 0));
        block.setFlags(m_layoutFlags);

        if (m_numInstances > 0)
        {
            block.setInstanceName("block");
            block.setArraySize(m_numInstances);
        }
    }

private:
    uint32_t m_layoutFlags;
    int m_numInstances;
};

class BlockMultiBasicTypesCase : public UniformBlockCase
{
public:
    BlockMultiBasicTypesCase(Context &context, const char *name, const char *description, uint32_t flagsA,
                             uint32_t flagsB, BufferMode bufferMode, int numInstances)
        : UniformBlockCase(context.getTestContext(), context.getRenderContext(), name, description,
                           glu::GLSL_VERSION_300_ES, bufferMode)
        , m_flagsA(flagsA)
        , m_flagsB(flagsB)
        , m_numInstances(numInstances)
    {
    }

    void init(void)
    {
        UniformBlock &blockA = m_interface.allocBlock("BlockA");
        blockA.addUniform(Uniform("a", VarType(glu::TYPE_FLOAT, PRECISION_HIGH)));
        blockA.addUniform(Uniform("b", VarType(glu::TYPE_UINT_VEC3, PRECISION_LOW), UNUSED_BOTH));
        blockA.addUniform(Uniform("c", VarType(glu::TYPE_FLOAT_MAT2, PRECISION_MEDIUM)));
        blockA.setInstanceName("blockA");
        blockA.setFlags(m_flagsA);

        UniformBlock &blockB = m_interface.allocBlock("BlockB");
        blockB.addUniform(Uniform("a", VarType(glu::TYPE_FLOAT_MAT3, PRECISION_MEDIUM)));
        blockB.addUniform(Uniform("b", VarType(glu::TYPE_INT_VEC2, PRECISION_LOW)));
        blockB.addUniform(Uniform("c", VarType(glu::TYPE_FLOAT_VEC4, PRECISION_HIGH), UNUSED_BOTH));
        blockB.addUniform(Uniform("d", VarType(glu::TYPE_BOOL, 0)));
        blockB.setInstanceName("blockB");
        blockB.setFlags(m_flagsB);

        if (m_numInstances > 0)
        {
            blockA.setArraySize(m_numInstances);
            blockB.setArraySize(m_numInstances);
        }
    }

private:
    uint32_t m_flagsA;
    uint32_t m_flagsB;
    int m_numInstances;
};

class BlockMultiNestedStructCase : public UniformBlockCase
{
public:
    BlockMultiNestedStructCase(Context &context, const char *name, const char *description, uint32_t flagsA,
                               uint32_t flagsB, BufferMode bufferMode, int numInstances)
        : UniformBlockCase(context.getTestContext(), context.getRenderContext(), name, description,
                           glu::GLSL_VERSION_300_ES, bufferMode)
        , m_flagsA(flagsA)
        , m_flagsB(flagsB)
        , m_numInstances(numInstances)
    {
    }

    void init(void)
    {
        StructType &typeS = m_interface.allocStruct("S");
        typeS.addMember("a", VarType(glu::TYPE_FLOAT_MAT3, PRECISION_LOW));
        typeS.addMember("b", VarType(VarType(glu::TYPE_INT_VEC2, PRECISION_MEDIUM), 4));
        typeS.addMember("c", VarType(glu::TYPE_FLOAT_VEC4, PRECISION_HIGH));

        StructType &typeT = m_interface.allocStruct("T");
        typeT.addMember("a", VarType(glu::TYPE_UINT, PRECISION_MEDIUM), UNUSED_BOTH);
        typeT.addMember("b", VarType(&typeS));
        typeT.addMember("c", VarType(glu::TYPE_BOOL_VEC4, 0));

        UniformBlock &blockA = m_interface.allocBlock("BlockA");
        blockA.addUniform(Uniform("a", VarType(glu::TYPE_FLOAT, PRECISION_HIGH)));
        blockA.addUniform(Uniform("b", VarType(&typeS)));
        blockA.addUniform(Uniform("c", VarType(glu::TYPE_UINT_VEC3, PRECISION_LOW), UNUSED_BOTH));
        blockA.setInstanceName("blockA");
        blockA.setFlags(m_flagsA);

        UniformBlock &blockB = m_interface.allocBlock("BlockB");
        blockB.addUniform(Uniform("a", VarType(glu::TYPE_FLOAT_MAT2, PRECISION_MEDIUM)));
        blockB.addUniform(Uniform("b", VarType(&typeT)));
        blockB.addUniform(Uniform("c", VarType(glu::TYPE_BOOL_VEC4, 0), UNUSED_BOTH));
        blockB.addUniform(Uniform("d", VarType(glu::TYPE_BOOL, 0)));
        blockB.setInstanceName("blockB");
        blockB.setFlags(m_flagsB);

        if (m_numInstances > 0)
        {
            blockA.setArraySize(m_numInstances);
            blockB.setArraySize(m_numInstances);
        }
    }

private:
    uint32_t m_flagsA;
    uint32_t m_flagsB;
    int m_numInstances;
};

UniformBlockTests::UniformBlockTests(Context &context) : TestCaseGroup(context, "ubo", "Uniform Block tests")
{
}

UniformBlockTests::~UniformBlockTests(void)
{
}

void UniformBlockTests::init(void)
{
    static const glu::DataType basicTypes[] = {
        glu::TYPE_FLOAT,        glu::TYPE_FLOAT_VEC2,   glu::TYPE_FLOAT_VEC3,   glu::TYPE_FLOAT_VEC4,
        glu::TYPE_INT,          glu::TYPE_INT_VEC2,     glu::TYPE_INT_VEC3,     glu::TYPE_INT_VEC4,
        glu::TYPE_UINT,         glu::TYPE_UINT_VEC2,    glu::TYPE_UINT_VEC3,    glu::TYPE_UINT_VEC4,
        glu::TYPE_BOOL,         glu::TYPE_BOOL_VEC2,    glu::TYPE_BOOL_VEC3,    glu::TYPE_BOOL_VEC4,
        glu::TYPE_FLOAT_MAT2,   glu::TYPE_FLOAT_MAT3,   glu::TYPE_FLOAT_MAT4,   glu::TYPE_FLOAT_MAT2X3,
        glu::TYPE_FLOAT_MAT2X4, glu::TYPE_FLOAT_MAT3X2, glu::TYPE_FLOAT_MAT3X4, glu::TYPE_FLOAT_MAT4X2,
        glu::TYPE_FLOAT_MAT4X3};

    static const struct
    {
        const char *name;
        uint32_t flags;
    } precisionFlags[] = {{"lowp", PRECISION_LOW}, {"mediump", PRECISION_MEDIUM}, {"highp", PRECISION_HIGH}};

    static const struct
    {
        const char *name;
        uint32_t flags;
    } layoutFlags[] = {{"shared", LAYOUT_SHARED}, {"packed", LAYOUT_PACKED}, {"std140", LAYOUT_STD140}};

    static const struct
    {
        const char *name;
        uint32_t flags;
    } matrixFlags[] = {{"row_major", LAYOUT_ROW_MAJOR}, {"column_major", LAYOUT_COLUMN_MAJOR}};

    static const struct
    {
        const char *name;
        UniformBlockCase::BufferMode mode;
    } bufferModes[] = {{"per_block_buffer", UniformBlockCase::BUFFERMODE_PER_BLOCK},
                       {"single_buffer", UniformBlockCase::BUFFERMODE_SINGLE}};

    // ubo.single_basic_type
    {
        tcu::TestCaseGroup *singleBasicTypeGroup =
            new tcu::TestCaseGroup(m_testCtx, "single_basic_type", "Single basic variable in single buffer");
        addChild(singleBasicTypeGroup);

        for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
        {
            tcu::TestCaseGroup *layoutGroup = new tcu::TestCaseGroup(m_testCtx, layoutFlags[layoutFlagNdx].name, "");
            singleBasicTypeGroup->addChild(layoutGroup);

            for (int basicTypeNdx = 0; basicTypeNdx < DE_LENGTH_OF_ARRAY(basicTypes); basicTypeNdx++)
            {
                glu::DataType type   = basicTypes[basicTypeNdx];
                const char *typeName = glu::getDataTypeName(type);

                if (glu::isDataTypeBoolOrBVec(type))
                    createBlockBasicTypeCases(layoutGroup, m_context, typeName, VarType(type, 0),
                                              layoutFlags[layoutFlagNdx].flags);
                else
                {
                    for (int precNdx = 0; precNdx < DE_LENGTH_OF_ARRAY(precisionFlags); precNdx++)
                        createBlockBasicTypeCases(
                            layoutGroup, m_context, (string(precisionFlags[precNdx].name) + "_" + typeName).c_str(),
                            VarType(type, precisionFlags[precNdx].flags), layoutFlags[layoutFlagNdx].flags);
                }

                if (glu::isDataTypeMatrix(type))
                {
                    for (int matFlagNdx = 0; matFlagNdx < DE_LENGTH_OF_ARRAY(matrixFlags); matFlagNdx++)
                    {
                        for (int precNdx = 0; precNdx < DE_LENGTH_OF_ARRAY(precisionFlags); precNdx++)
                            createBlockBasicTypeCases(layoutGroup, m_context,
                                                      (string(matrixFlags[matFlagNdx].name) + "_" +
                                                       precisionFlags[precNdx].name + "_" + typeName)
                                                          .c_str(),
                                                      VarType(type, precisionFlags[precNdx].flags),
                                                      layoutFlags[layoutFlagNdx].flags | matrixFlags[matFlagNdx].flags);
                    }
                }
            }
        }
    }

    // ubo.single_basic_array
    {
        tcu::TestCaseGroup *singleBasicArrayGroup =
            new tcu::TestCaseGroup(m_testCtx, "single_basic_array", "Single basic array variable in single buffer");
        addChild(singleBasicArrayGroup);

        for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
        {
            tcu::TestCaseGroup *layoutGroup = new tcu::TestCaseGroup(m_testCtx, layoutFlags[layoutFlagNdx].name, "");
            singleBasicArrayGroup->addChild(layoutGroup);

            for (int basicTypeNdx = 0; basicTypeNdx < DE_LENGTH_OF_ARRAY(basicTypes); basicTypeNdx++)
            {
                glu::DataType type   = basicTypes[basicTypeNdx];
                const char *typeName = glu::getDataTypeName(type);
                const int arraySize  = 3;

                createBlockBasicTypeCases(
                    layoutGroup, m_context, typeName,
                    VarType(VarType(type, glu::isDataTypeBoolOrBVec(type) ? 0 : PRECISION_HIGH), arraySize),
                    layoutFlags[layoutFlagNdx].flags);

                if (glu::isDataTypeMatrix(type))
                {
                    for (int matFlagNdx = 0; matFlagNdx < DE_LENGTH_OF_ARRAY(matrixFlags); matFlagNdx++)
                        createBlockBasicTypeCases(layoutGroup, m_context,
                                                  (string(matrixFlags[matFlagNdx].name) + "_" + typeName).c_str(),
                                                  VarType(VarType(type, PRECISION_HIGH), arraySize),
                                                  layoutFlags[layoutFlagNdx].flags | matrixFlags[matFlagNdx].flags);
                }
            }
        }
    }

    // ubo.single_struct
    {
        tcu::TestCaseGroup *singleStructGroup =
            new tcu::TestCaseGroup(m_testCtx, "single_struct", "Single struct in uniform block");
        addChild(singleStructGroup);

        for (int modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(bufferModes); modeNdx++)
        {
            tcu::TestCaseGroup *modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferModes[modeNdx].name, "");
            singleStructGroup->addChild(modeGroup);

            for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
            {
                for (int isArray = 0; isArray < 2; isArray++)
                {
                    std::string baseName = layoutFlags[layoutFlagNdx].name;
                    uint32_t baseFlags   = layoutFlags[layoutFlagNdx].flags;

                    if (bufferModes[modeNdx].mode == UniformBlockCase::BUFFERMODE_SINGLE && isArray == 0)
                        continue; // Doesn't make sense to add this variant.

                    if (isArray)
                        baseName += "_instance_array";

                    modeGroup->addChild(new BlockSingleStructCase(m_context, (baseName + "_vertex").c_str(), "",
                                                                  baseFlags | DECLARE_VERTEX, bufferModes[modeNdx].mode,
                                                                  isArray ? 3 : 0));
                    modeGroup->addChild(new BlockSingleStructCase(m_context, (baseName + "_fragment").c_str(), "",
                                                                  baseFlags | DECLARE_FRAGMENT,
                                                                  bufferModes[modeNdx].mode, isArray ? 3 : 0));

                    if (!(baseFlags & LAYOUT_PACKED))
                        modeGroup->addChild(new BlockSingleStructCase(m_context, (baseName + "_both").c_str(), "",
                                                                      baseFlags | DECLARE_VERTEX | DECLARE_FRAGMENT,
                                                                      bufferModes[modeNdx].mode, isArray ? 3 : 0));
                }
            }
        }
    }

    // ubo.single_struct_array
    {
        tcu::TestCaseGroup *singleStructArrayGroup =
            new tcu::TestCaseGroup(m_testCtx, "single_struct_array", "Struct array in one uniform block");
        addChild(singleStructArrayGroup);

        for (int modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(bufferModes); modeNdx++)
        {
            tcu::TestCaseGroup *modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferModes[modeNdx].name, "");
            singleStructArrayGroup->addChild(modeGroup);

            for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
            {
                for (int isArray = 0; isArray < 2; isArray++)
                {
                    std::string baseName = layoutFlags[layoutFlagNdx].name;
                    uint32_t baseFlags   = layoutFlags[layoutFlagNdx].flags;

                    if (bufferModes[modeNdx].mode == UniformBlockCase::BUFFERMODE_SINGLE && isArray == 0)
                        continue; // Doesn't make sense to add this variant.

                    if (isArray)
                        baseName += "_instance_array";

                    modeGroup->addChild(new BlockSingleStructArrayCase(m_context, (baseName + "_vertex").c_str(), "",
                                                                       baseFlags | DECLARE_VERTEX,
                                                                       bufferModes[modeNdx].mode, isArray ? 3 : 0));
                    modeGroup->addChild(new BlockSingleStructArrayCase(m_context, (baseName + "_fragment").c_str(), "",
                                                                       baseFlags | DECLARE_FRAGMENT,
                                                                       bufferModes[modeNdx].mode, isArray ? 3 : 0));

                    if (!(baseFlags & LAYOUT_PACKED))
                        modeGroup->addChild(new BlockSingleStructArrayCase(
                            m_context, (baseName + "_both").c_str(), "", baseFlags | DECLARE_VERTEX | DECLARE_FRAGMENT,
                            bufferModes[modeNdx].mode, isArray ? 3 : 0));
                }
            }
        }
    }

    // ubo.single_nested_struct
    {
        tcu::TestCaseGroup *singleNestedStructGroup =
            new tcu::TestCaseGroup(m_testCtx, "single_nested_struct", "Nested struct in one uniform block");
        addChild(singleNestedStructGroup);

        for (int modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(bufferModes); modeNdx++)
        {
            tcu::TestCaseGroup *modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferModes[modeNdx].name, "");
            singleNestedStructGroup->addChild(modeGroup);

            for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
            {
                for (int isArray = 0; isArray < 2; isArray++)
                {
                    std::string baseName = layoutFlags[layoutFlagNdx].name;
                    uint32_t baseFlags   = layoutFlags[layoutFlagNdx].flags;

                    if (bufferModes[modeNdx].mode == UniformBlockCase::BUFFERMODE_SINGLE && isArray == 0)
                        continue; // Doesn't make sense to add this variant.

                    if (isArray)
                        baseName += "_instance_array";

                    modeGroup->addChild(new BlockSingleNestedStructCase(m_context, (baseName + "_vertex").c_str(), "",
                                                                        baseFlags | DECLARE_VERTEX,
                                                                        bufferModes[modeNdx].mode, isArray ? 3 : 0));
                    modeGroup->addChild(new BlockSingleNestedStructCase(m_context, (baseName + "_fragment").c_str(), "",
                                                                        baseFlags | DECLARE_FRAGMENT,
                                                                        bufferModes[modeNdx].mode, isArray ? 3 : 0));

                    if (!(baseFlags & LAYOUT_PACKED))
                        modeGroup->addChild(new BlockSingleNestedStructCase(
                            m_context, (baseName + "_both").c_str(), "", baseFlags | DECLARE_VERTEX | DECLARE_FRAGMENT,
                            bufferModes[modeNdx].mode, isArray ? 3 : 0));
                }
            }
        }
    }

    // ubo.single_nested_struct_mixed_matrix_packing
    {
        tcu::TestCaseGroup *singleNestedStructMixedMatrixPackingGroup =
            new tcu::TestCaseGroup(m_testCtx, "single_nested_struct_mixed_matrix_packing",
                                   "Nested struct in one uniform block with a mixed matrix packing");
        addChild(singleNestedStructMixedMatrixPackingGroup);

        for (const auto &bufferMode : bufferModes)
        {
            tcu::TestCaseGroup *modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferMode.name, "");
            singleNestedStructMixedMatrixPackingGroup->addChild(modeGroup);

            for (const auto &layoutFlag : layoutFlags)
                for (const auto &blockMatrixFlag : matrixFlags)
                    for (const auto &singleMatrixFlag : matrixFlags)
                        for (const auto &arrayMatrixFlag : matrixFlags)
                            for (int isArray = 0; isArray < 2; isArray++)
                            {
                                std::string baseName = layoutFlag.name;
                                uint32_t baseFlags   = layoutFlag.flags;
                                uint32_t blockFlags  = baseFlags | blockMatrixFlag.flags;

                                baseName += std::string("_block_") + blockMatrixFlag.name;
                                baseName += std::string("_matrix_") + singleMatrixFlag.name;
                                baseName += std::string("_matrixarray_") + arrayMatrixFlag.name;

                                if (bufferMode.mode == UniformBlockCase::BUFFERMODE_SINGLE && isArray == 0)
                                    continue; // Doesn't make sense to add this variant.

                                if (isArray)
                                    baseName += "_instance_array";

                                modeGroup->addChild(new BlockSingleNestedStructMixedMatrixPackingCase(
                                    m_context, (baseName + "_vertex").c_str(), "", blockFlags | DECLARE_VERTEX,
                                    singleMatrixFlag.flags, arrayMatrixFlag.flags, bufferMode.mode, isArray ? 3 : 0));
                                modeGroup->addChild(new BlockSingleNestedStructMixedMatrixPackingCase(
                                    m_context, (baseName + "_fragment").c_str(), "", blockFlags | DECLARE_FRAGMENT,
                                    singleMatrixFlag.flags, arrayMatrixFlag.flags, bufferMode.mode, isArray ? 3 : 0));

                                if (!(baseFlags & LAYOUT_PACKED))
                                    modeGroup->addChild(new BlockSingleNestedStructMixedMatrixPackingCase(
                                        m_context, (baseName + "_both").c_str(), "",
                                        blockFlags | DECLARE_VERTEX | DECLARE_FRAGMENT, singleMatrixFlag.flags,
                                        arrayMatrixFlag.flags, bufferMode.mode, isArray ? 3 : 0));
                            }
        }
    }

    // ubo.single_nested_struct_array
    {
        tcu::TestCaseGroup *singleNestedStructArrayGroup =
            new tcu::TestCaseGroup(m_testCtx, "single_nested_struct_array", "Nested struct array in one uniform block");
        addChild(singleNestedStructArrayGroup);

        for (int modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(bufferModes); modeNdx++)
        {
            tcu::TestCaseGroup *modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferModes[modeNdx].name, "");
            singleNestedStructArrayGroup->addChild(modeGroup);

            for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
            {
                for (int isArray = 0; isArray < 2; isArray++)
                {
                    std::string baseName = layoutFlags[layoutFlagNdx].name;
                    uint32_t baseFlags   = layoutFlags[layoutFlagNdx].flags;

                    if (bufferModes[modeNdx].mode == UniformBlockCase::BUFFERMODE_SINGLE && isArray == 0)
                        continue; // Doesn't make sense to add this variant.

                    if (isArray)
                        baseName += "_instance_array";

                    modeGroup->addChild(new BlockSingleNestedStructArrayCase(
                        m_context, (baseName + "_vertex").c_str(), "", baseFlags | DECLARE_VERTEX,
                        bufferModes[modeNdx].mode, isArray ? 3 : 0));
                    modeGroup->addChild(new BlockSingleNestedStructArrayCase(
                        m_context, (baseName + "_fragment").c_str(), "", baseFlags | DECLARE_FRAGMENT,
                        bufferModes[modeNdx].mode, isArray ? 3 : 0));

                    if (!(baseFlags & LAYOUT_PACKED))
                        modeGroup->addChild(new BlockSingleNestedStructArrayCase(
                            m_context, (baseName + "_both").c_str(), "", baseFlags | DECLARE_VERTEX | DECLARE_FRAGMENT,
                            bufferModes[modeNdx].mode, isArray ? 3 : 0));
                }
            }
        }
    }

    // ubo.instance_array_basic_type
    {
        tcu::TestCaseGroup *instanceArrayBasicTypeGroup =
            new tcu::TestCaseGroup(m_testCtx, "instance_array_basic_type", "Single basic variable in instance array");
        addChild(instanceArrayBasicTypeGroup);

        for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
        {
            tcu::TestCaseGroup *layoutGroup = new tcu::TestCaseGroup(m_testCtx, layoutFlags[layoutFlagNdx].name, "");
            instanceArrayBasicTypeGroup->addChild(layoutGroup);

            for (int basicTypeNdx = 0; basicTypeNdx < DE_LENGTH_OF_ARRAY(basicTypes); basicTypeNdx++)
            {
                glu::DataType type     = basicTypes[basicTypeNdx];
                const char *typeName   = glu::getDataTypeName(type);
                const int numInstances = 3;

                createBlockBasicTypeCases(layoutGroup, m_context, typeName,
                                          VarType(type, glu::isDataTypeBoolOrBVec(type) ? 0 : PRECISION_HIGH),
                                          layoutFlags[layoutFlagNdx].flags, numInstances);

                if (glu::isDataTypeMatrix(type))
                {
                    for (int matFlagNdx = 0; matFlagNdx < DE_LENGTH_OF_ARRAY(matrixFlags); matFlagNdx++)
                        createBlockBasicTypeCases(
                            layoutGroup, m_context, (string(matrixFlags[matFlagNdx].name) + "_" + typeName).c_str(),
                            VarType(type, PRECISION_HIGH),
                            layoutFlags[layoutFlagNdx].flags | matrixFlags[matFlagNdx].flags, numInstances);
                }
            }
        }
    }

    // ubo.multi_basic_types
    {
        tcu::TestCaseGroup *multiBasicTypesGroup =
            new tcu::TestCaseGroup(m_testCtx, "multi_basic_types", "Multiple buffers with basic types");
        addChild(multiBasicTypesGroup);

        for (int modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(bufferModes); modeNdx++)
        {
            tcu::TestCaseGroup *modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferModes[modeNdx].name, "");
            multiBasicTypesGroup->addChild(modeGroup);

            for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
            {
                for (int isArray = 0; isArray < 2; isArray++)
                {
                    std::string baseName = layoutFlags[layoutFlagNdx].name;
                    uint32_t baseFlags   = layoutFlags[layoutFlagNdx].flags;

                    if (isArray)
                        baseName += "_instance_array";

                    modeGroup->addChild(new BlockMultiBasicTypesCase(
                        m_context, (baseName + "_vertex").c_str(), "", baseFlags | DECLARE_VERTEX,
                        baseFlags | DECLARE_VERTEX, bufferModes[modeNdx].mode, isArray ? 3 : 0));
                    modeGroup->addChild(new BlockMultiBasicTypesCase(
                        m_context, (baseName + "_fragment").c_str(), "", baseFlags | DECLARE_FRAGMENT,
                        baseFlags | DECLARE_FRAGMENT, bufferModes[modeNdx].mode, isArray ? 3 : 0));

                    if (!(baseFlags & LAYOUT_PACKED))
                        modeGroup->addChild(new BlockMultiBasicTypesCase(
                            m_context, (baseName + "_both").c_str(), "", baseFlags | DECLARE_VERTEX | DECLARE_FRAGMENT,
                            baseFlags | DECLARE_VERTEX | DECLARE_FRAGMENT, bufferModes[modeNdx].mode, isArray ? 3 : 0));

                    modeGroup->addChild(new BlockMultiBasicTypesCase(
                        m_context, (baseName + "_mixed").c_str(), "", baseFlags | DECLARE_VERTEX,
                        baseFlags | DECLARE_FRAGMENT, bufferModes[modeNdx].mode, isArray ? 3 : 0));
                }
            }
        }
    }

    // ubo.multi_nested_struct
    {
        tcu::TestCaseGroup *multiNestedStructGroup =
            new tcu::TestCaseGroup(m_testCtx, "multi_nested_struct", "Multiple buffers with nested structs");
        addChild(multiNestedStructGroup);

        for (int modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(bufferModes); modeNdx++)
        {
            tcu::TestCaseGroup *modeGroup = new tcu::TestCaseGroup(m_testCtx, bufferModes[modeNdx].name, "");
            multiNestedStructGroup->addChild(modeGroup);

            for (int layoutFlagNdx = 0; layoutFlagNdx < DE_LENGTH_OF_ARRAY(layoutFlags); layoutFlagNdx++)
            {
                for (int isArray = 0; isArray < 2; isArray++)
                {
                    std::string baseName = layoutFlags[layoutFlagNdx].name;
                    uint32_t baseFlags   = layoutFlags[layoutFlagNdx].flags;

                    if (isArray)
                        baseName += "_instance_array";

                    modeGroup->addChild(new BlockMultiNestedStructCase(
                        m_context, (baseName + "_vertex").c_str(), "", baseFlags | DECLARE_VERTEX,
                        baseFlags | DECLARE_VERTEX, bufferModes[modeNdx].mode, isArray ? 3 : 0));
                    modeGroup->addChild(new BlockMultiNestedStructCase(
                        m_context, (baseName + "_fragment").c_str(), "", baseFlags | DECLARE_FRAGMENT,
                        baseFlags | DECLARE_FRAGMENT, bufferModes[modeNdx].mode, isArray ? 3 : 0));

                    if (!(baseFlags & LAYOUT_PACKED))
                        modeGroup->addChild(new BlockMultiNestedStructCase(
                            m_context, (baseName + "_both").c_str(), "", baseFlags | DECLARE_VERTEX | DECLARE_FRAGMENT,
                            baseFlags | DECLARE_VERTEX | DECLARE_FRAGMENT, bufferModes[modeNdx].mode, isArray ? 3 : 0));

                    modeGroup->addChild(new BlockMultiNestedStructCase(
                        m_context, (baseName + "_mixed").c_str(), "", baseFlags | DECLARE_VERTEX,
                        baseFlags | DECLARE_FRAGMENT, bufferModes[modeNdx].mode, isArray ? 3 : 0));
                }
            }
        }
    }

    // ubo.random
    {
        const uint32_t allShaders    = FEATURE_VERTEX_BLOCKS | FEATURE_FRAGMENT_BLOCKS | FEATURE_SHARED_BLOCKS;
        const uint32_t allLayouts    = FEATURE_PACKED_LAYOUT | FEATURE_SHARED_LAYOUT | FEATURE_STD140_LAYOUT;
        const uint32_t allBasicTypes = FEATURE_VECTORS | FEATURE_MATRICES;
        const uint32_t unused        = FEATURE_UNUSED_MEMBERS | FEATURE_UNUSED_UNIFORMS;
        const uint32_t matFlags      = FEATURE_MATRIX_LAYOUT;
        const uint32_t allFeatures   = ~FEATURE_ARRAYS_OF_ARRAYS;

        tcu::TestCaseGroup *randomGroup = new tcu::TestCaseGroup(m_testCtx, "random", "Random Uniform Block cases");
        addChild(randomGroup);

        // Basic types.
        createRandomCaseGroup(randomGroup, m_context, "scalar_types", "Scalar types only, per-block buffers",
                              UniformBlockCase::BUFFERMODE_PER_BLOCK, allShaders | allLayouts | unused, 25, 0);
        createRandomCaseGroup(randomGroup, m_context, "vector_types", "Scalar and vector types only, per-block buffers",
                              UniformBlockCase::BUFFERMODE_PER_BLOCK,
                              allShaders | allLayouts | unused | FEATURE_VECTORS, 25, 25);
        createRandomCaseGroup(randomGroup, m_context, "basic_types", "All basic types, per-block buffers",
                              UniformBlockCase::BUFFERMODE_PER_BLOCK,
                              allShaders | allLayouts | unused | allBasicTypes | matFlags, 25, 50);
        createRandomCaseGroup(randomGroup, m_context, "basic_arrays", "Arrays, per-block buffers",
                              UniformBlockCase::BUFFERMODE_PER_BLOCK,
                              allShaders | allLayouts | unused | allBasicTypes | matFlags | FEATURE_ARRAYS, 25, 50);

        createRandomCaseGroup(randomGroup, m_context, "basic_instance_arrays",
                              "Basic instance arrays, per-block buffers", UniformBlockCase::BUFFERMODE_PER_BLOCK,
                              allShaders | allLayouts | unused | allBasicTypes | matFlags | FEATURE_INSTANCE_ARRAYS, 25,
                              75);
        createRandomCaseGroup(randomGroup, m_context, "nested_structs", "Nested structs, per-block buffers",
                              UniformBlockCase::BUFFERMODE_PER_BLOCK,
                              allShaders | allLayouts | unused | allBasicTypes | matFlags | FEATURE_STRUCTS, 25, 100);
        createRandomCaseGroup(
            randomGroup, m_context, "nested_structs_arrays", "Nested structs, arrays, per-block buffers",
            UniformBlockCase::BUFFERMODE_PER_BLOCK,
            allShaders | allLayouts | unused | allBasicTypes | matFlags | FEATURE_STRUCTS | FEATURE_ARRAYS, 25, 150);
        createRandomCaseGroup(
            randomGroup, m_context, "nested_structs_instance_arrays",
            "Nested structs, instance arrays, per-block buffers", UniformBlockCase::BUFFERMODE_PER_BLOCK,
            allShaders | allLayouts | unused | allBasicTypes | matFlags | FEATURE_STRUCTS | FEATURE_INSTANCE_ARRAYS, 25,
            125);
        createRandomCaseGroup(randomGroup, m_context, "nested_structs_arrays_instance_arrays",
                              "Nested structs, instance arrays, per-block buffers",
                              UniformBlockCase::BUFFERMODE_PER_BLOCK,
                              allShaders | allLayouts | unused | allBasicTypes | matFlags | FEATURE_STRUCTS |
                                  FEATURE_ARRAYS | FEATURE_INSTANCE_ARRAYS,
                              25, 175);

        createRandomCaseGroup(randomGroup, m_context, "all_per_block_buffers", "All random features, per-block buffers",
                              UniformBlockCase::BUFFERMODE_PER_BLOCK, allFeatures, 50, 200);
        createRandomCaseGroup(randomGroup, m_context, "all_shared_buffer", "All random features, shared buffer",
                              UniformBlockCase::BUFFERMODE_SINGLE, allFeatures, 50, 250);
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
