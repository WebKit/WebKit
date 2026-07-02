#ifndef _ES3FFBOMULTISAMPLETESTS_HPP
#define _ES3FFBOMULTISAMPLETESTS_HPP
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
 * \brief FBO multisample tests.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tes3TestCase.hpp"

namespace deqp
{
namespace gles3
{
namespace Functional
{

class FboMultisampleTests : public TestCaseGroup
{
public:
    FboMultisampleTests(Context &context);
    ~FboMultisampleTests(void);

    void init(void);

private:
    FboMultisampleTests(const FboMultisampleTests &other);
    FboMultisampleTests &operator=(const FboMultisampleTests &other);
};

} // namespace Functional
} // namespace gles3
} // namespace deqp

#endif // _ES3FFBOMULTISAMPLETESTS_HPP
