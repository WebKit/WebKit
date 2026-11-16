#ifndef _TCULNXX11GLXPLATFORM_HPP
#define _TCULNXX11GLXPLATFORM_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief Platform that uses X11 via GLX.
 *//*--------------------------------------------------------------------*/

#include "gluContextFactory.hpp"
#include "deUniquePtr.hpp"
#include "tcuLnxX11.hpp"

namespace tcu
{
namespace lnx
{
namespace x11
{
namespace glx
{

de::MovePtr<glu::ContextFactory> createContextFactory(EventState &eventState);

} // namespace glx
} // namespace x11
} // namespace lnx
} // namespace tcu

#endif // _TCULNXX11GLXPLATFORM_HPP
