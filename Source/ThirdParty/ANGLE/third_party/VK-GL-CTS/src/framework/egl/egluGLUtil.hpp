#ifndef _EGLUGLUTIL_HPP
#define _EGLUGLUTIL_HPP
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
 * \brief EGL utilities for interfacing with GL APIs.
 *//*--------------------------------------------------------------------*/

#include "egluDefs.hpp"
#include "eglwDefs.hpp"
#include "eglwEnums.hpp"

#include "gluRenderConfig.hpp"
#include "glwDefs.hpp"

namespace eglw
{
class Library;
}

namespace eglu
{

glw::GLenum getImageGLTarget(eglw::EGLenum source);
eglw::EGLint apiRenderableType(glu::ApiType apiType);

eglw::EGLContext createGLContext(
    const eglw::Library &egl, eglw::EGLDisplay display, eglw::EGLConfig config, const glu::ContextType &contextType,
    eglw::EGLContext sharedContext                           = EGL_NO_CONTEXT,
    glu::ResetNotificationStrategy resetNotificationStrategy = glu::RESET_NOTIFICATION_STRATEGY_NOT_SPECIFIED);

eglw::EGLConfig chooseConfig(const eglw::Library &egl, eglw::EGLDisplay display, const glu::RenderConfig &config);

} // namespace eglu

#endif // _EGLUGLUTIL_HPP
