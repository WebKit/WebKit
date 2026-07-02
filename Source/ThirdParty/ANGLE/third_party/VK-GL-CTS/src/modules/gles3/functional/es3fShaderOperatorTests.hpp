#ifndef _ES3FSHADEROPERATORTESTS_HPP
#define _ES3FSHADEROPERATORTESTS_HPP
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
 * \brief Shader operators tests.
 *//*--------------------------------------------------------------------*/

#include "tes3TestCase.hpp"

namespace deqp
{
namespace gles3
{
namespace Functional
{

class ShaderOperatorTests : public TestCaseGroup
{
public:
    ShaderOperatorTests(Context &context);
    virtual ~ShaderOperatorTests(void);

    virtual void init(void);

private:
    ShaderOperatorTests(const ShaderOperatorTests &);            // not allowed!
    ShaderOperatorTests &operator=(const ShaderOperatorTests &); // not allowed!
};

} // namespace Functional
} // namespace gles3
} // namespace deqp

#endif // _ES3FSHADEROPERATORTESTS_HPP
