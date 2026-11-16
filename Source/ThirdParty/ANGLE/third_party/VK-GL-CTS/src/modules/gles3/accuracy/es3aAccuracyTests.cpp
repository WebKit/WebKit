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
 * \brief Accuracy tests.
 *//*--------------------------------------------------------------------*/

#include "es3aAccuracyTests.hpp"
#include "es3aTextureFilteringTests.hpp"
#include "es3aTextureMipmapTests.hpp"
#include "es3aVaryingInterpolationTests.hpp"

namespace deqp
{
namespace gles3
{
namespace Accuracy
{

class TextureTests : public TestCaseGroup
{
public:
    TextureTests(Context &context) : TestCaseGroup(context, "texture", "Texturing Accuracy Tests")
    {
    }

    void init(void)
    {
        addChild(new TextureFilteringTests(m_context));
        addChild(new TextureMipmapTests(m_context));
    }
};

AccuracyTests::AccuracyTests(Context &context) : TestCaseGroup(context, "accuracy", "Accuracy Tests")
{
}

AccuracyTests::~AccuracyTests(void)
{
}

void AccuracyTests::init(void)
{
    addChild(new VaryingInterpolationTests(m_context));
    addChild(new TextureTests(m_context));
}

} // namespace Accuracy
} // namespace gles3
} // namespace deqp
