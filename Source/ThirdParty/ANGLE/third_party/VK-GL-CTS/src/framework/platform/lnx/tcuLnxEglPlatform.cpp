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

#include "tcuLnxEglPlatform.hpp"

#if defined(DEQP_SUPPORT_X11)
#include "tcuLnxX11EglDisplayFactory.hpp"
#endif // DEQP_SUPPORT_X11

#if defined(DEQP_SUPPORT_WAYLAND)
#include "tcuLnxWaylandEglDisplayFactory.hpp"
#endif // DEQP_SUPPORT_WAYLAND

#include "egluGLContextFactory.hpp"

namespace tcu
{
namespace lnx
{
namespace egl
{

Platform::Platform(EventState &eventState)
{
#if defined(DEQP_SUPPORT_X11)
    m_nativeDisplayFactoryRegistry.registerFactory(x11::egl::createDisplayFactory(eventState));
#endif // DEQP_SUPPORT_X11

#if defined(DEQP_SUPPORT_WAYLAND)
    m_nativeDisplayFactoryRegistry.registerFactory(wayland::egl::createDisplayFactory(eventState));
#endif // DEQP_SUPPORT_WAYLAND
}

de::MovePtr<glu::ContextFactory> Platform::createContextFactory(void)
{
    return de::MovePtr<glu::ContextFactory>(new eglu::GLContextFactory(m_nativeDisplayFactoryRegistry));
}

} // namespace egl
} // namespace lnx
} // namespace tcu
