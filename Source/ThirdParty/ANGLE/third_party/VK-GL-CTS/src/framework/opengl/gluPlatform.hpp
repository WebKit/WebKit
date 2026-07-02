#ifndef _GLUPLATFORM_HPP
#define _GLUPLATFORM_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
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
 * \brief OpenGL platform interface.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "gluContextFactory.hpp"

namespace tcu
{
class CommandLine;
}

namespace glu
{

/*--------------------------------------------------------------------*//*!
 * \brief OpenGL (ES) platform interface
 *
 * OpenGL (ES) platform interface provides abstraction for GL context
 * creation. A single platform may support multiple methods for creating
 * rendering contexts (such as GLX and EGL on Linux). This is accomplished
 * by having a list of GL context factories (m_contextFactoryRegistry).
 *
 * Context factory is selected on run-time using --deqp-gl-context-type
 * command line argument. If no such argument is given, first entry
 * is used as default.
 *
 * See glu::ContextFactory and glu::RenderContext for complete details
 * on the porting layer.
 *
 * If your platform supports EGL and you have implemented eglu::Platform,
 * you may enable EGL support just by registering eglu::GLContextFactory.
 *
 * OpenGL (ES) platform implementation is required by OpenGL (ES) tests.
 * It is NOT required by EGL or OpenCL tests.
 *//*--------------------------------------------------------------------*/
class Platform
{
public:
    Platform(void);
    ~Platform(void);

    const ContextFactoryRegistry &getContextFactoryRegistry(void) const
    {
        return m_contextFactoryRegistry;
    }

protected:
    //! GL context factory registry. Add your context factories here in constructor.
    ContextFactoryRegistry m_contextFactoryRegistry;
};

} // namespace glu

#endif // _GLUPLATFORM_HPP
