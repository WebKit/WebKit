#ifndef _ES3FSHADERCONSTEXPRTESTS_HPP
#define _ES3FSHADERCONSTEXPRTESTS_HPP
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
 * \brief GLES3 shader constant expression tests
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tes3TestCase.hpp"

namespace deqp
{
namespace gles3
{
namespace Functional
{

class ShaderConstExprTests : public TestCaseGroup
{
public:
    ShaderConstExprTests(Context &context);
    virtual ~ShaderConstExprTests(void);

    void init(void);

private:
    ShaderConstExprTests(const ShaderConstExprTests &other);
    ShaderConstExprTests &operator=(const ShaderConstExprTests &other);
};

} // namespace Functional
} // namespace gles3
} // namespace deqp

#endif // _ES3FSHADERCONSTEXPRTESTS_HPP
