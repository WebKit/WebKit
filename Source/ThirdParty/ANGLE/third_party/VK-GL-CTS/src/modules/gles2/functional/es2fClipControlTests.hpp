#ifndef _ES2FCLIPCONTROLTESTS_HPP
#define _ES2FCLIPCONTROLTESTS_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2014-2016 The Khronos Group Inc.
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
 */ /*!
 * \file
 * \brief
 */ /*-------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tes2TestCase.hpp"

namespace deqp
{
namespace gles2
{
namespace Functional
{

class ClipControlTests : public TestCaseGroup
{
public:
    ClipControlTests(Context &context);
    ~ClipControlTests(void);
    void init(void);

private:
    ClipControlTests(const ClipControlTests &other);
    ClipControlTests &operator=(const ClipControlTests &other);
};

} // namespace Functional
} // namespace gles2
} // namespace deqp

#endif // _ES2FCLIPCONTROLTESTS_HPP
