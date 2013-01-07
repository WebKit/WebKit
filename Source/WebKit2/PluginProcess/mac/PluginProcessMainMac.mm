/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
#import "PluginProcessMain.h"

#if ENABLE(PLUGIN_PROCESS)

#import "ChildProcessMain.h"
#import "EnvironmentUtilities.h"
#import "NetscapePluginModule.h"
#import "PluginProcess.h"

namespace WebKit {

class PluginProcessMainDelegate : public ChildProcessMainDelegate {
public:
    PluginProcessMainDelegate(const CommandLine& commandLine)
        : ChildProcessMainDelegate(commandLine)
    {
    }

    virtual void doPreInitializationWork()
    {
        // Remove the PluginProcess shim from the DYLD_INSERT_LIBRARIES environment variable so any processes
        // spawned by the PluginProcess don't try to insert the shim and crash.
        EnvironmentUtilities::stripValuesEndingWithString("DYLD_INSERT_LIBRARIES", "/PluginProcessShim.dylib");

        // Check if we're being spawned to write a MIME type preferences file.
        String pluginPath = m_commandLine["createPluginMIMETypesPreferences"];
        if (!pluginPath.isEmpty()) {
            if (!NetscapePluginModule::createPluginMIMETypesPreferences(pluginPath))
                exit(EXIT_FAILURE);
            exit(EXIT_SUCCESS);
        }
    }

    virtual void doPostRunWork()
    {
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
        // If we have private temporary and cache directories, clean them up.
        if (getenv("DIRHELPER_USER_DIR_SUFFIX")) {
            char darwinDirectory[PATH_MAX];
            if (confstr(_CS_DARWIN_USER_TEMP_DIR, darwinDirectory, sizeof(darwinDirectory)))
                [[NSFileManager defaultManager] removeItemAtPath:[[NSFileManager defaultManager] stringWithFileSystemRepresentation:darwinDirectory length:strlen(darwinDirectory)] error:nil];
            if (confstr(_CS_DARWIN_USER_CACHE_DIR, darwinDirectory, sizeof(darwinDirectory)))
                [[NSFileManager defaultManager] removeItemAtPath:[[NSFileManager defaultManager] stringWithFileSystemRepresentation:darwinDirectory length:strlen(darwinDirectory)] error:nil];
        }
#endif
    }
};

int PluginProcessMain(const CommandLine& commandLine)
{
    return ChildProcessMain<PluginProcess, PluginProcessMainDelegate>(commandLine);
}

}

#endif // ENABLE(PLUGIN_PROCESS)
