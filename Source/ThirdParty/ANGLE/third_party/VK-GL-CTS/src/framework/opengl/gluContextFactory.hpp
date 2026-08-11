#ifndef _GLUCONTEXTFACTORY_HPP
#define _GLUCONTEXTFACTORY_HPP
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
 * \brief Base class for GLContext factories.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuFactoryRegistry.hpp"
#include "gluRenderContext.hpp"
#include "gluRenderConfig.hpp"

namespace tcu
{
class CommandLine;
}

namespace glu
{

/*--------------------------------------------------------------------*//*!
 * \brief OpenGL (ES) context factory
 *
 * In order to support OpenGL (ES) tests, glu::Platform implementation
 * must provide at least one GL context factory.
 *//*--------------------------------------------------------------------*/
class ContextFactory : public tcu::FactoryBase
{
public:
    ContextFactory(const std::string &name, const std::string &description);
    virtual ~ContextFactory(void);

    /*--------------------------------------------------------------------*//*!
     * Create OpenGL rendering context.
     *
     * GL(ES) tests will call this when entering TestPackage (i.e. when launching
     * test execution session) and destroy the context upon leaving TestPackage.
     *
     * Only single context will be active concurrently and it will be accessed
     * only from the calling thread.
     *
     * \param config        Rendering context configuration
     * \param cmdLine        Command line for extra arguments
     * \param sharedContext    Context with which objects should be shared
     * \return Rendering context wrapper object.
     *//*--------------------------------------------------------------------*/
    virtual RenderContext *createContext(const RenderConfig &config, const tcu::CommandLine &cmdLine,
                                         const glu::RenderContext *sharedContext) const = 0;

private:
    ContextFactory(const ContextFactory &);
    ContextFactory &operator=(const ContextFactory &);
};

typedef tcu::FactoryRegistry<ContextFactory> ContextFactoryRegistry;

} // namespace glu

#endif // _GLUCONTEXTFACTORY_HPP
