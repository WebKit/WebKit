#ifndef _EGLUPLATFORM_HPP
#define _EGLUPLATFORM_HPP
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
 * \brief EGL platform interface.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "egluNativeDisplay.hpp"
#include "gluRenderContext.hpp"

namespace tcu
{
class CommandLine;
class FunctionLibrary;
} // namespace tcu

namespace eglu
{

/*--------------------------------------------------------------------*//*!
 * \brief EGL platform interface
 *
 * EGL platform interface provides mechanism to implement platform-specific
 * bits of EGL API for use in EGL tests, or OpenGL (ES) context creation.
 *
 * A single platform can support multiple native object types. This is
 * accomplished by registering multiple object factories. Command line
 * parameters (such as --deqp-egl-display-type=) are used to select
 * object types.
 *
 * See following classes for complete description of the porting layer:
 *
 *  * eglu::NativeDisplay, created by eglu::NativeDisplayFactory
 *  * eglu::NativeWindow, created by eglu::NativeWindowFactory
 *  * eglu::NativePixmap, created by eglu::NativePixmapFactory
 *
 * If you implement EGL support, you may use it to enable GL support by
 * adding eglu::GLContextFactory to m_contextFactoryRegistry in your
 * glu::Platform implementation.
 *
 * EGL platform implementation is required by EGL tests. OpenGL (ES) and
 * OpenCL tests can benefit from, but do not require EGL platform
 * implementation.
 *//*--------------------------------------------------------------------*/
class Platform
{
public:
    Platform(void);
    // Code outside porting layer will never attempt to delete eglu::Platform
    virtual ~Platform(void);

    const NativeDisplayFactoryRegistry &getNativeDisplayFactoryRegistry(void) const
    {
        return m_nativeDisplayFactoryRegistry;
    }

    /*--------------------------------------------------------------------*//*!
     * \brief Get fallback GL library
     *
     * EGL tests use eglGetProcAddress() to load API entry points. However,
     * if the platform does not support EGL_KHR_get_all_proc_addresses extension,
     * core API entry points must be loaded using alternative method, namely
     * this default GL function library.
     *
     * You may implement platform-specific way for loading GL entry points
     * by implementing this method.
     *
     * Default implementation provides entry points for ES2 and ES3 APIs
     * if binary is directly linked against GLES library.
     *
     * \param contextType    GL context type
     * \param cmdLine        Reserved for future use
     *//*--------------------------------------------------------------------*/
    virtual tcu::FunctionLibrary *createDefaultGLFunctionLibrary(glu::ApiType apiType,
                                                                 const tcu::CommandLine &cmdLine) const;

protected:
    /*--------------------------------------------------------------------*//*!
     * \brief Native display factory registry
     *
     * Native display factory registry holds list of eglu::NativeDisplayFactory
     * objects that can create eglu::NativeDisplay instances. You should
     * implement eglu::NativeDisplay and eglu::NativeDisplayFactory and add
     * instance of that factory implementation to this registry.
     *
     * --deqp-egl-display-type command line argument is used to select the
     * display factory to use. If no type is given in command line, first entry
     * is used.
     *//*--------------------------------------------------------------------*/
    NativeDisplayFactoryRegistry m_nativeDisplayFactoryRegistry;
};

} // namespace eglu

#endif // _EGLUPLATFORM_HPP
