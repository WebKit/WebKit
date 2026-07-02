#ifndef _ES2FTEXTUREWRAPTESTS_HPP
#define _ES2FTEXTUREWRAPTESTS_HPP
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
 * \brief Texture wrap mode tests.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tes2TestCase.hpp"

namespace deqp
{
namespace gles2
{
namespace Functional
{

class TextureWrapTests : public TestCaseGroup
{
public:
    TextureWrapTests(Context &context);
    ~TextureWrapTests(void);

    void init(void);

private:
    TextureWrapTests(const TextureWrapTests &other);
    TextureWrapTests &operator=(const TextureWrapTests &other);
};

} // namespace Functional
} // namespace gles2
} // namespace deqp

#endif // _ES2FTEXTUREWRAPTESTS_HPP
