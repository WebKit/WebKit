/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
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
 * \brief Random uniform block layout case.
 *//*--------------------------------------------------------------------*/

#include "glsRandomUniformBlockCase.hpp"
#include "tcuCommandLine.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"

using std::string;
using std::vector;

namespace deqp
{
namespace gls
{

using namespace gls::ub;

RandomUniformBlockCase::RandomUniformBlockCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                               glu::GLSLVersion glslVersion, const char *name, const char *description,
                                               BufferMode bufferMode, uint32_t features, uint32_t seed)
    : UniformBlockCase(testCtx, renderCtx, name, description, glslVersion, bufferMode)
    , m_features(features)
    , m_maxVertexBlocks((features & FEATURE_VERTEX_BLOCKS) ? 4 : 0)
    , m_maxFragmentBlocks((features & FEATURE_FRAGMENT_BLOCKS) ? 4 : 0)
    , m_maxSharedBlocks((features & FEATURE_SHARED_BLOCKS) ? 4 : 0)
    , m_maxInstances((features & FEATURE_INSTANCE_ARRAYS) ? 3 : 0)
    , m_maxArrayLength((features & FEATURE_ARRAYS) ? 8 : 0)
    , m_maxStructDepth((features & FEATURE_STRUCTS) ? 2 : 0)
    , m_maxBlockMembers(5)
    , m_maxStructMembers(4)
    , m_seed(seed)
    , m_blockNdx(1)
    , m_uniformNdx(1)
    , m_structNdx(1)
{
}

void RandomUniformBlockCase::init(void)
{
    de::Random rnd(m_seed);

    int numShared     = m_maxSharedBlocks > 0 ? rnd.getInt(1, m_maxSharedBlocks) : 0;
    int numVtxBlocks  = m_maxVertexBlocks - numShared > 0 ? rnd.getInt(1, m_maxVertexBlocks - numShared) : 0;
    int numFragBlocks = m_maxFragmentBlocks - numShared > 0 ? rnd.getInt(1, m_maxFragmentBlocks - numShared) : 0;

    for (int ndx = 0; ndx < numShared; ndx++)
        generateBlock(rnd, DECLARE_VERTEX | DECLARE_FRAGMENT);

    for (int ndx = 0; ndx < numVtxBlocks; ndx++)
        generateBlock(rnd, DECLARE_VERTEX);

    for (int ndx = 0; ndx < numFragBlocks; ndx++)
        generateBlock(rnd, DECLARE_FRAGMENT);
}

void RandomUniformBlockCase::generateBlock(de::Random &rnd, uint32_t layoutFlags)
{
    DE_ASSERT(m_blockNdx <= 'z' - 'a');

    const float instanceArrayWeight = 0.3f;
    UniformBlock &block             = m_interface.allocBlock((string("Block") + (char)('A' + m_blockNdx)).c_str());
    int numInstances = (m_maxInstances > 0 && rnd.getFloat() < instanceArrayWeight) ? rnd.getInt(0, m_maxInstances) : 0;
    int numUniforms  = rnd.getInt(1, m_maxBlockMembers);

    if (numInstances > 0)
        block.setArraySize(numInstances);

    if (numInstances > 0 || rnd.getBool())
        block.setInstanceName((string("block") + (char)('A' + m_blockNdx)).c_str());

    // Layout flag candidates.
    vector<uint32_t> layoutFlagCandidates;
    layoutFlagCandidates.push_back(0);
    if (m_features & FEATURE_PACKED_LAYOUT)
        layoutFlagCandidates.push_back(LAYOUT_SHARED);
    if ((m_features & FEATURE_SHARED_LAYOUT) && ((layoutFlags & DECLARE_BOTH) != DECLARE_BOTH))
        layoutFlagCandidates.push_back(LAYOUT_PACKED); // \note packed layout can only be used in a single shader stage.
    if (m_features & FEATURE_STD140_LAYOUT)
        layoutFlagCandidates.push_back(LAYOUT_STD140);

    layoutFlags |= rnd.choose<uint32_t>(layoutFlagCandidates.begin(), layoutFlagCandidates.end());

    if (m_features & FEATURE_MATRIX_LAYOUT)
    {
        static const uint32_t matrixCandidates[] = {0, LAYOUT_ROW_MAJOR, LAYOUT_COLUMN_MAJOR};
        layoutFlags |=
            rnd.choose<uint32_t>(&matrixCandidates[0], &matrixCandidates[DE_LENGTH_OF_ARRAY(matrixCandidates)]);
    }

    block.setFlags(layoutFlags);

    for (int ndx = 0; ndx < numUniforms; ndx++)
        generateUniform(rnd, block);

    m_blockNdx += 1;
}

static std::string genName(char first, char last, int ndx)
{
    std::string str = "";
    int alphabetLen = last - first + 1;

    while (ndx > alphabetLen)
    {
        str.insert(str.begin(), (char)(first + ((ndx - 1) % alphabetLen)));
        ndx = ((ndx - 1) / alphabetLen);
    }

    str.insert(str.begin(), (char)(first + (ndx % (alphabetLen + 1)) - 1));

    return str;
}

void RandomUniformBlockCase::generateUniform(de::Random &rnd, UniformBlock &block)
{
    const float unusedVtxWeight  = 0.15f;
    const float unusedFragWeight = 0.15f;
    bool unusedOk                = (m_features & FEATURE_UNUSED_UNIFORMS) != 0;
    uint32_t flags               = 0;
    std::string name             = genName('a', 'z', m_uniformNdx);
    VarType type                 = generateType(rnd, 0, true);

    flags |= (unusedOk && rnd.getFloat() < unusedVtxWeight) ? UNUSED_VERTEX : 0;
    flags |= (unusedOk && rnd.getFloat() < unusedFragWeight) ? UNUSED_FRAGMENT : 0;

    block.addUniform(Uniform(name.c_str(), type, flags));

    m_uniformNdx += 1;
}

VarType RandomUniformBlockCase::generateType(de::Random &rnd, int typeDepth, bool arrayOk)
{
    const float structWeight = 0.1f;
    const float arrayWeight  = 0.1f;

    if (typeDepth < m_maxStructDepth && rnd.getFloat() < structWeight)
    {
        const float unusedVtxWeight  = 0.15f;
        const float unusedFragWeight = 0.15f;
        bool unusedOk                = (m_features & FEATURE_UNUSED_MEMBERS) != 0;
        vector<VarType> memberTypes;
        int numMembers = rnd.getInt(1, m_maxStructMembers);

        // Generate members first so nested struct declarations are in correct order.
        for (int ndx = 0; ndx < numMembers; ndx++)
            memberTypes.push_back(generateType(rnd, typeDepth + 1, true));

        StructType &structType = m_interface.allocStruct((string("s") + genName('A', 'Z', m_structNdx)).c_str());
        m_structNdx += 1;

        DE_ASSERT(numMembers <= 'Z' - 'A');
        for (int ndx = 0; ndx < numMembers; ndx++)
        {
            uint32_t flags = 0;

            flags |= (unusedOk && rnd.getFloat() < unusedVtxWeight) ? UNUSED_VERTEX : 0;
            flags |= (unusedOk && rnd.getFloat() < unusedFragWeight) ? UNUSED_FRAGMENT : 0;

            structType.addMember((string("m") + (char)('A' + ndx)).c_str(), memberTypes[ndx], flags);
        }

        return VarType(&structType);
    }
    else if (m_maxArrayLength > 0 && arrayOk && rnd.getFloat() < arrayWeight)
    {
        const bool arraysOfArraysOk = (m_features & FEATURE_ARRAYS_OF_ARRAYS) != 0;
        const int arrayLength       = rnd.getInt(1, m_maxArrayLength);
        VarType elementType         = generateType(rnd, typeDepth, arraysOfArraysOk);
        return VarType(elementType, arrayLength);
    }
    else
    {
        vector<glu::DataType> typeCandidates;

        typeCandidates.push_back(glu::TYPE_FLOAT);
        typeCandidates.push_back(glu::TYPE_INT);
        typeCandidates.push_back(glu::TYPE_UINT);
        typeCandidates.push_back(glu::TYPE_BOOL);

        if (m_features & FEATURE_VECTORS)
        {
            typeCandidates.push_back(glu::TYPE_FLOAT_VEC2);
            typeCandidates.push_back(glu::TYPE_FLOAT_VEC3);
            typeCandidates.push_back(glu::TYPE_FLOAT_VEC4);
            typeCandidates.push_back(glu::TYPE_INT_VEC2);
            typeCandidates.push_back(glu::TYPE_INT_VEC3);
            typeCandidates.push_back(glu::TYPE_INT_VEC4);
            typeCandidates.push_back(glu::TYPE_UINT_VEC2);
            typeCandidates.push_back(glu::TYPE_UINT_VEC3);
            typeCandidates.push_back(glu::TYPE_UINT_VEC4);
            typeCandidates.push_back(glu::TYPE_BOOL_VEC2);
            typeCandidates.push_back(glu::TYPE_BOOL_VEC3);
            typeCandidates.push_back(glu::TYPE_BOOL_VEC4);
        }

        if (m_features & FEATURE_MATRICES)
        {
            typeCandidates.push_back(glu::TYPE_FLOAT_MAT2);
            typeCandidates.push_back(glu::TYPE_FLOAT_MAT2X3);
            typeCandidates.push_back(glu::TYPE_FLOAT_MAT3X2);
            typeCandidates.push_back(glu::TYPE_FLOAT_MAT3);
            typeCandidates.push_back(glu::TYPE_FLOAT_MAT3X4);
            typeCandidates.push_back(glu::TYPE_FLOAT_MAT4X2);
            typeCandidates.push_back(glu::TYPE_FLOAT_MAT4X3);
            typeCandidates.push_back(glu::TYPE_FLOAT_MAT4);
        }

        glu::DataType type = rnd.choose<glu::DataType>(typeCandidates.begin(), typeCandidates.end());
        uint32_t flags     = 0;

        if (!glu::isDataTypeBoolOrBVec(type))
        {
            // Precision.
            static const uint32_t precisionCandidates[] = {PRECISION_LOW, PRECISION_MEDIUM, PRECISION_HIGH};
            flags |= rnd.choose<uint32_t>(&precisionCandidates[0],
                                          &precisionCandidates[DE_LENGTH_OF_ARRAY(precisionCandidates)]);
        }

        return VarType(type, flags);
    }
}

} // namespace gls
} // namespace deqp
