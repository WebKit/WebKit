#ifndef _GLSRANDOMUNIFORMBLOCKCASE_HPP
#define _GLSRANDOMUNIFORMBLOCKCASE_HPP
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

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "glsUniformBlockCase.hpp"

namespace de
{
class Random;
} // namespace de

namespace deqp
{
namespace gls
{

namespace ub
{

enum FeatureBits
{
    FEATURE_VECTORS          = (1 << 0),
    FEATURE_MATRICES         = (1 << 1),
    FEATURE_ARRAYS           = (1 << 2),
    FEATURE_STRUCTS          = (1 << 3),
    FEATURE_NESTED_STRUCTS   = (1 << 4),
    FEATURE_INSTANCE_ARRAYS  = (1 << 5),
    FEATURE_VERTEX_BLOCKS    = (1 << 6),
    FEATURE_FRAGMENT_BLOCKS  = (1 << 7),
    FEATURE_SHARED_BLOCKS    = (1 << 8),
    FEATURE_UNUSED_UNIFORMS  = (1 << 9),
    FEATURE_UNUSED_MEMBERS   = (1 << 10),
    FEATURE_PACKED_LAYOUT    = (1 << 11),
    FEATURE_SHARED_LAYOUT    = (1 << 12),
    FEATURE_STD140_LAYOUT    = (1 << 13),
    FEATURE_MATRIX_LAYOUT    = (1 << 14), //!< Matrix layout flags.
    FEATURE_ARRAYS_OF_ARRAYS = (1 << 15)
};

} // namespace ub

class RandomUniformBlockCase : public UniformBlockCase
{
public:
    RandomUniformBlockCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, glu::GLSLVersion glslVersion,
                           const char *name, const char *description, BufferMode bufferMode, uint32_t features,
                           uint32_t seed);

    void init(void);

private:
    void generateBlock(de::Random &rnd, uint32_t layoutFlags);
    void generateUniform(de::Random &rnd, ub::UniformBlock &block);
    ub::VarType generateType(de::Random &rnd, int typeDepth, bool arrayOk);

    const uint32_t m_features;
    const int m_maxVertexBlocks;
    const int m_maxFragmentBlocks;
    const int m_maxSharedBlocks;
    const int m_maxInstances;
    const int m_maxArrayLength;
    const int m_maxStructDepth;
    const int m_maxBlockMembers;
    const int m_maxStructMembers;
    const uint32_t m_seed;

    int m_blockNdx;
    int m_uniformNdx;
    int m_structNdx;
};

} // namespace gls
} // namespace deqp

#endif // _GLSRANDOMUNIFORMBLOCKCASE_HPP
