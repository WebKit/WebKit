/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "NetscapePluginModule.h"

#include "Module.h"
#include "NetscapeBrowserFuncs.h"
#include <wtf/PassOwnPtr.h>

namespace WebKit {

static Vector<NetscapePluginModule*>& initializedNetscapePluginModules()
{
    DEFINE_STATIC_LOCAL(Vector<NetscapePluginModule*>, initializedNetscapePluginModules, ());
    return initializedNetscapePluginModules;
}

NetscapePluginModule::NetscapePluginModule(const String& pluginPath)
    : m_pluginPath(pluginPath)
    , m_isInitialized(false)
    , m_pluginCount(0)
    , m_shutdownProcPtr(0)
    , m_pluginFuncs()
{
}

NetscapePluginModule::~NetscapePluginModule()
{
    ASSERT(initializedNetscapePluginModules().find(this) == notFound);
}

void NetscapePluginModule::pluginCreated()
{
    if (!m_pluginCount) {
        // Load the plug-in module if necessary.
        load();
    }
        
    m_pluginCount++;
}

void NetscapePluginModule::pluginDestroyed()
{
    ASSERT(m_pluginCount > 0);
    m_pluginCount--;
    
    if (!m_pluginCount) {
        shutdown();
        unload();
    }
}

void NetscapePluginModule::shutdown()
{
    ASSERT(m_isInitialized);

    m_shutdownProcPtr();

    m_isInitialized = false;

    size_t pluginModuleIndex = initializedNetscapePluginModules().find(this);
    ASSERT(pluginModuleIndex != notFound);
    
    initializedNetscapePluginModules().remove(pluginModuleIndex);
}

PassRefPtr<NetscapePluginModule> NetscapePluginModule::getOrCreate(const String& pluginPath)
{
    // First, see if we already have a module with this plug-in path.
    for (size_t i = 0; i < initializedNetscapePluginModules().size(); ++i) {
        NetscapePluginModule* pluginModule = initializedNetscapePluginModules()[i];

        if (pluginModule->m_pluginPath == pluginPath)
            return pluginModule;
    }

    RefPtr<NetscapePluginModule> pluginModule(adoptRef(new NetscapePluginModule(pluginPath)));
    
    // Try to load and initialize the plug-in module.
    if (!pluginModule->load())
        return 0;
    
    return pluginModule.release();
}

bool NetscapePluginModule::load()
{
    if (m_isInitialized) {
        ASSERT(initializedNetscapePluginModules().find(this) != notFound);
        return true;
    }

    if (!tryLoad()) {
        unload();
        return false;
    }
    
    m_isInitialized = true;

    ASSERT(initializedNetscapePluginModules().find(this) == notFound);
    initializedNetscapePluginModules().append(this);

    determineQuirks();

    return true;
}

bool NetscapePluginModule::tryLoad()
{
    m_module = adoptPtr(new Module(m_pluginPath));
    if (!m_module->load())
        return false;

    NP_InitializeFuncPtr initializeFuncPtr = m_module->functionPointer<NP_InitializeFuncPtr>("NP_Initialize");
    if (!initializeFuncPtr)
        return false;

    NP_GetEntryPointsFuncPtr getEntryPointsFuncPtr = m_module->functionPointer<NP_GetEntryPointsFuncPtr>("NP_GetEntryPoints");
    if (!getEntryPointsFuncPtr)
        return false;

    m_shutdownProcPtr = m_module->functionPointer<NPP_ShutdownProcPtr>("NP_Shutdown");
    if (!m_shutdownProcPtr)
        return false;

    m_pluginFuncs.size = sizeof(NPPluginFuncs);
    m_pluginFuncs.version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;

    // On Mac, NP_Initialize must be called first, then NP_GetEntryPoints. On Windows, the order is
    // reversed. Failing to follow this order results in crashes (e.g., in Silverlight on Mac and
    // in Flash and QuickTime on Windows).
#if PLATFORM(MAC)
    if (initializeFuncPtr(netscapeBrowserFuncs()) != NPERR_NO_ERROR || getEntryPointsFuncPtr(&m_pluginFuncs) != NPERR_NO_ERROR)
        return false;
#elif PLATFORM(WIN)
    if (getEntryPointsFuncPtr(&m_pluginFuncs) != NPERR_NO_ERROR || initializeFuncPtr(netscapeBrowserFuncs()) != NPERR_NO_ERROR)
        return false;
#endif

    return true;
}

void NetscapePluginModule::unload()
{
    ASSERT(!m_isInitialized);

    m_module = 0;
}

} // namespace WebKit

