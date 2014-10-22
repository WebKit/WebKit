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
#if PLUGIN_ARCHITECTURE(X11) && ENABLE(NETSCAPE_PLUGIN_API)

#include "NetscapePluginModule.h"

#include "PluginProcessProxy.h"
#include "NetscapeBrowserFuncs.h"
#include <WebCore/FileSystem.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wtf/text/StringBuilder.h>

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


void NetscapePluginModule::parseMIMEDescription(const String& mimeDescription, Vector<MimeClassInfo>& result)
{
    ASSERT_ARG(result, result.isEmpty());

    Vector<String> types;
    mimeDescription.lower().split(UChar(';'), false, types);
    result.reserveInitialCapacity(types.size());

    size_t mimeInfoCount = 0;
    for (size_t i = 0; i < types.size(); ++i) {
        Vector<String> mimeTypeParts;
        types[i].split(UChar(':'), true, mimeTypeParts);
        if (mimeTypeParts.size() <= 0)
            continue;

        result.uncheckedAppend(MimeClassInfo());
        MimeClassInfo& mimeInfo = result[mimeInfoCount++];
        mimeInfo.type = mimeTypeParts[0];

        if (mimeTypeParts.size() > 1)
            mimeTypeParts[1].split(UChar(','), false, mimeInfo.extensions);

        if (mimeTypeParts.size() > 2)
            mimeInfo.desc = mimeTypeParts[2];
    }
}

String NetscapePluginModule::buildMIMEDescription(const Vector<MimeClassInfo>& mimeDescription)
{
    StringBuilder builder;

    size_t mimeInfoCount = mimeDescription.size();
    for (size_t i = 0; i < mimeInfoCount; ++i) {
        const MimeClassInfo& mimeInfo = mimeDescription[i];
        builder.append(mimeInfo.type);
        builder.append(':');

        size_t extensionsCount = mimeInfo.extensions.size();
        for (size_t j = 0; j < extensionsCount; ++j) {
            builder.append(mimeInfo.extensions[j]);
            if (j != extensionsCount - 1)
                builder.append(',');
        }
        builder.append(':');

        builder.append(mimeInfo.desc);
        if (i != mimeInfoCount - 1)
            builder.append(';');
    }

    return builder.toString();
}

bool NetscapePluginModule::getPluginInfoForLoadedPlugin(RawPluginMetaData& metaData)
{
    ASSERT(m_isInitialized);

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
        metaData.name = String::fromUTF8(buffer);

    error = NPP_GetValue(0, NPPVpluginDescriptionString, &buffer);
    if (error == NPERR_NO_ERROR)
        metaData.description = String::fromUTF8(buffer);

    String mimeDescription = String::fromUTF8(NP_GetMIMEDescription());
    if (mimeDescription.isNull())
        return false;

    metaData.mimeDescription = mimeDescription;

    return true;
}

bool NetscapePluginModule::getPluginInfo(const String& pluginPath, PluginModuleInfo& plugin)
{
    RawPluginMetaData metaData;
    if (!PluginProcessProxy::scanPlugin(pluginPath, metaData))
        return false;

    plugin.path = pluginPath;
    plugin.info.file = pathGetFileName(pluginPath);
    plugin.info.name = metaData.name;
    plugin.info.desc = metaData.description;
    parseMIMEDescription(metaData.mimeDescription, plugin.info.mimes);

    return true;
}

void NetscapePluginModule::determineQuirks()
{
#if CPU(X86_64)
    RawPluginMetaData metaData;
    if (!getPluginInfoForLoadedPlugin(metaData))
        return;

    Vector<MimeClassInfo> mimeTypes;
    parseMIMEDescription(metaData.mimeDescription, mimeTypes);
    for (size_t i = 0; i < mimeTypes.size(); ++i) {
        if (mimeTypes[i].type == "application/x-shockwave-flash") {
            m_pluginQuirks.add(PluginQuirks::IgnoreRightClickInWindowlessMode);
            m_pluginQuirks.add(PluginQuirks::DoNotCancelSrcStreamInWindowedMode);
            break;
        }
    }
#endif
}

static void writeCharacter(char byte)
{
    int result;
    while ((result = fputc(byte, stdout)) == EOF && errno == EINTR) { }
    ASSERT(result != EOF);
}

static void writeLine(const String& line)
{
    CString utf8String = line.utf8();
    const char* utf8Data = utf8String.data();

    for (unsigned i = 0; i < utf8String.length(); i++) {
        char character = utf8Data[i];
        if (character != '\n')
            writeCharacter(character);
    }
    writeCharacter('\n');
}

bool NetscapePluginModule::scanPlugin(const String& pluginPath)
{
    RawPluginMetaData metaData;

    {
        // Don't allow the plugin to pollute the standard output.
        StdoutDevNullRedirector stdOutRedirector;

        // We are loading the plugin here since it does not seem to be a standardized way to
        // get the needed informations from a UNIX plugin without loading it.
        RefPtr<NetscapePluginModule> pluginModule = NetscapePluginModule::getOrCreate(pluginPath);
        if (!pluginModule)
            return false;

        pluginModule->incrementLoadCount();
        bool success = pluginModule->getPluginInfoForLoadedPlugin(metaData);
        pluginModule->decrementLoadCount();

        if (!success)
            return false;
    }

    // Write data to standard output for the UI process.
    writeLine(metaData.name);
    writeLine(metaData.description);
    writeLine(metaData.mimeDescription);

    fflush(stdout);

    return true;
}

} // namespace WebKit

#endif // PLUGIN_ARCHITECTURE(X11) && ENABLE(NETSCAPE_PLUGIN_API)
