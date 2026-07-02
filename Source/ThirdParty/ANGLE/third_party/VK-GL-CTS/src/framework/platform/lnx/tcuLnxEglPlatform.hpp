#ifndef _TCULNXEGLPLATFORM_HPP
#define _TCULNXEGLPLATFORM_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Linux EGL Platform.
 *//*--------------------------------------------------------------------*/

#include "tcuLnx.hpp"
#include "egluPlatform.hpp"
#include "deUniquePtr.hpp"
#include "gluContextFactory.hpp"

namespace tcu
{
namespace lnx
{
namespace egl
{

class Platform : public eglu::Platform
{
public:
    Platform(EventState &eventState);
    ~Platform(void)
    {
    }

    de::MovePtr<glu::ContextFactory> createContextFactory(void);
};

} // namespace egl
} // namespace lnx
} // namespace tcu

#endif // _TCULNXEGLPLATFORM_HPP
