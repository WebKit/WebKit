#ifndef _EGLUGLCONTEXTFACTORY_HPP
#define _EGLUGLCONTEXTFACTORY_HPP
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
 * \brief GL context factory using EGL.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "gluContextFactory.hpp"
#include "egluNativeDisplay.hpp"

namespace tcu
{
class Platform;
} // namespace tcu

namespace eglu
{

//! EGL-specific render context interface - used in OpenCL interop
class GLRenderContext : public glu::RenderContext
{
public:
    virtual eglw::EGLDisplay getEGLDisplay(void) const  = 0;
    virtual eglw::EGLContext getEGLContext(void) const  = 0;
    virtual eglw::EGLConfig getEGLConfig(void) const    = 0;
    virtual const eglw::Library &getLibrary(void) const = 0;
};

class GLContextFactory : public glu::ContextFactory
{
public:
    GLContextFactory(const NativeDisplayFactoryRegistry &displayFactoryRegistry);
    virtual glu::RenderContext *createContext(const glu::RenderConfig &config, const tcu::CommandLine &cmdLine,
                                              const glu::RenderContext *sharedContext) const;

private:
    const NativeDisplayFactoryRegistry &m_displayFactoryRegistry;
};

} // namespace eglu

#endif // _EGLUGLCONTEXTFACTORY_HPP
