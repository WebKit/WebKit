/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#import "config.h"

#import "EnvironmentUtilities.h"
#import "PluginProcess.h"
#import "WKBase.h"
#import "XPCServiceEntryPoint.h"
#import <wtf/RunLoop.h>

#if ENABLE(NETSCAPE_PLUGIN_API)

namespace WebKit {

class PluginServiceInitializerDelegate : public XPCServiceInitializerDelegate {
public:
    PluginServiceInitializerDelegate(OSObjectPtr<xpc_connection_t> connection, xpc_object_t initializerMessage)
        : XPCServiceInitializerDelegate(WTFMove(connection), initializerMessage)
    {
    }

    virtual bool getExtraInitializationData(HashMap<String, String>& extraInitializationData)
    {
        xpc_object_t extraDataInitializationDataObject = xpc_dictionary_get_value(m_initializerMessage, "extra-initialization-data");

        String pluginPath = xpc_dictionary_get_string(extraDataInitializationDataObject, "plugin-path");
        if (pluginPath.isEmpty())
            return false;
        extraInitializationData.add("plugin-path", pluginPath);

        String disableSandbox = xpc_dictionary_get_string(extraDataInitializationDataObject, "disable-sandbox");
        if (!disableSandbox.isEmpty())
            extraInitializationData.add("disable-sandbox", disableSandbox);

        String experimentalSandboxPlugIn = xpc_dictionary_get_string(extraDataInitializationDataObject, "experimental-sandbox-plugin");
        if (!experimentalSandboxPlugIn.isEmpty())
            extraInitializationData.add("experimental-sandbox-plugin"_s, experimentalSandboxPlugIn);

        return true;
    }
};

} // namespace WebKit

using namespace WebKit;

#endif // ENABLE(NETSCAPE_PLUGIN_API)

extern "C" WK_EXPORT void PluginServiceInitializer(xpc_connection_t connection, xpc_object_t initializerMessage, xpc_object_t priorityBoostMessage);

void PluginServiceInitializer(xpc_connection_t connection, xpc_object_t initializerMessage, xpc_object_t priorityBoostMessage)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    // FIXME: Add support for teardown from PluginProcessMain.mm

    // Remove the PluginProcess shim from the DYLD_INSERT_LIBRARIES environment variable so any processes
    // spawned by the PluginProcess don't try to insert the shim and crash.
    EnvironmentUtilities::removeValuesEndingWith("DYLD_INSERT_LIBRARIES", "/PluginProcessShim.dylib");
    XPCServiceInitializer<PluginProcess, PluginServiceInitializerDelegate>(adoptOSObject(connection), initializerMessage, priorityBoostMessage);
#endif // ENABLE(NETSCAPE_PLUGIN_API)
}
