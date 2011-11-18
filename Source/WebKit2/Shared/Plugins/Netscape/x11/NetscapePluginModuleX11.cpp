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

#include "config.h"
#if PLUGIN_ARCHITECTURE(X11)

#include "NetscapePluginModule.h"

#include "NetscapeBrowserFuncs.h"
#include <WebCore/FileSystem.h>

#if PLATFORM(QT)
#include <QLibrary>
#endif

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace WebCore;

namespace WebKit {

class StdoutDevNullRedirector {
public:
    StdoutDevNullRedirector();
    ~StdoutDevNullRedirector();

private:
    int m_savedStdout;
};

StdoutDevNullRedirector::StdoutDevNullRedirector()
    : m_savedStdout(-1)
{
    int newStdout = open("/dev/null", O_WRONLY);
    if (newStdout == -1)
        return;
    m_savedStdout = dup(STDOUT_FILENO);
    dup2(newStdout, STDOUT_FILENO);
}

StdoutDevNullRedirector::~StdoutDevNullRedirector()
{
    if (m_savedStdout != -1)
        dup2(m_savedStdout, STDOUT_FILENO);
}

#if PLATFORM(QT)
static void initializeGTK()
{
    QLibrary library(QLatin1String("libgtk-x11-2.0.so.0"));
    if (library.load()) {
        typedef void *(*gtk_init_check_ptr)(int*, char***);
        gtk_init_check_ptr gtkInitCheck = reinterpret_cast<gtk_init_check_ptr>(library.resolve("gtk_init_check"));
        // NOTE: We're using gtk_init_check() since gtk_init() calls exit() on failure.
        if (gtkInitCheck)
            (void) gtkInitCheck(0, 0);
    }
}
#endif

void NetscapePluginModule::applyX11QuirksBeforeLoad()
{
#if PLATFORM(QT)
    if (m_pluginPath.contains("npwrapper") || m_pluginPath.contains("flashplayer")) {
        initializeGTK();
        m_pluginQuirks.add(PluginQuirks::RequiresGTKToolKit);
    }
#endif
}

void NetscapePluginModule::setMIMEDescription(const String& mimeDescription, PluginModuleInfo& plugin)
{
    Vector<String> types;
    mimeDescription.lower().split(UChar(';'), false, types);
    plugin.info.mimes.reserveCapacity(types.size());

    size_t mimeInfoCount = 0;
    for (size_t i = 0; i < types.size(); ++i) {
        Vector<String> mimeTypeParts;
        types[i].split(UChar(':'), true, mimeTypeParts);
        if (mimeTypeParts.size() <= 0)
            continue;

        plugin.info.mimes.uncheckedAppend(MimeClassInfo());
        MimeClassInfo& mimeInfo = plugin.info.mimes[mimeInfoCount++];
        mimeInfo.type = mimeTypeParts[0];

        if (mimeTypeParts.size() > 1)
            mimeTypeParts[1].split(UChar(','), false, mimeInfo.extensions);

        if (mimeTypeParts.size() > 2)
            mimeInfo.desc = mimeTypeParts[2];
    }
}

bool NetscapePluginModule::getPluginInfoForLoadedPlugin(PluginModuleInfo& plugin)
{
    ASSERT(m_isInitialized);

    plugin.path = m_pluginPath;
    plugin.info.file = pathGetFileName(m_pluginPath);

    Module* module = m_module.get();
    NPP_GetValueProcPtr NPP_GetValue = module->functionPointer<NPP_GetValueProcPtr>("NP_GetValue");
    if (!NPP_GetValue)
        return false;

    NP_GetMIMEDescriptionFuncPtr NP_GetMIMEDescription = module->functionPointer<NP_GetMIMEDescriptionFuncPtr>("NP_GetMIMEDescription");
    if (!NP_GetMIMEDescription)
        return false;

    char* buffer;
    NPError error = NPP_GetValue(0, NPPVpluginNameString, &buffer);
    if (error == NPERR_NO_ERROR)
        plugin.info.name = buffer;

    error = NPP_GetValue(0, NPPVpluginDescriptionString, &buffer);
    if (error == NPERR_NO_ERROR)
        plugin.info.desc = buffer;

    const char* mimeDescription = NP_GetMIMEDescription();
    if (!mimeDescription)
        return false;

    setMIMEDescription(mimeDescription, plugin);

    return true;
}
bool NetscapePluginModule::getPluginInfo(const String& pluginPath, PluginModuleInfo& plugin)
{
    // Tempararily suppress stdout in this function as plugins will be loaded and shutdown and debug info
    // is leaked to layout test output.
    StdoutDevNullRedirector stdoutDevNullRedirector;

    // We are loading the plugin here since it does not seem to be a standardized way to
    // get the needed informations from a UNIX plugin without loading it.
    RefPtr<NetscapePluginModule> pluginModule = NetscapePluginModule::getOrCreate(pluginPath);
    if (!pluginModule)
        return false;

    pluginModule->incrementLoadCount();
    bool returnValue = pluginModule->getPluginInfoForLoadedPlugin(plugin);
    pluginModule->decrementLoadCount();

    return returnValue;
}

void NetscapePluginModule::determineQuirks()
{
#if CPU(X86_64)
    PluginModuleInfo plugin;
    if (!getPluginInfoForLoadedPlugin(plugin))
        return;

    Vector<MimeClassInfo> mimeTypes = plugin.info.mimes;
    for (size_t i = 0; i < mimeTypes.size(); ++i) {
        if (mimeTypes[i].type == "application/x-shockwave-flash") {
            m_pluginQuirks.add(PluginQuirks::IgnoreRightClickInWindowlessMode);
            break;
        }
    }
#endif
}

} // namespace WebKit

#endif // PLUGIN_ARCHITECTURE(X11)
