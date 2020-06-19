/*
 * Copyright (C) 2011, 2014 Igalia S.L.
 * Copyright (C) 2011 Apple Inc.
 * Copyright (C) 2012 Samsung Electronics
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PluginProcessProxy.h"

#if ENABLE(PLUGIN_PROCESS)

#include "PluginProcessCreationParameters.h"
#include "ProcessExecutablePath.h"
#include <WebCore/PlatformDisplay.h>
#include <sys/wait.h>
#include <wtf/FileSystem.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(GTK)
#include <glib.h>
#include <wtf/glib/GUniquePtr.h>
#endif

#if PLATFORM(GTK)
#include "Module.h"
#endif

namespace WebKit {
using namespace WebCore;

void PluginProcessProxy::platformGetLaunchOptionsWithAttributes(ProcessLauncher::LaunchOptions& launchOptions, const PluginProcessAttributes& pluginProcessAttributes)
{
    launchOptions.processType = ProcessLauncher::ProcessType::Plugin;
    launchOptions.extraInitializationData.add("plugin-path", pluginProcessAttributes.moduleInfo.path);
}

void PluginProcessProxy::platformInitializePluginProcess(PluginProcessCreationParameters&)
{
}

#if PLATFORM(GTK)
static bool pluginRequiresGtk2(const String& pluginPath)
{
    std::unique_ptr<Module> module = makeUnique<Module>(pluginPath);
    if (!module->load())
        return false;
    return module->functionPointer<gpointer>("gtk_object_get_type");
}
#endif

#if PLUGIN_ARCHITECTURE(UNIX)
bool PluginProcessProxy::scanPlugin(const String& pluginPath, RawPluginMetaData& result)
{
    String pluginProcessPath = executablePathOfPluginProcess();

#if PLATFORM(GTK)
    if (pluginRequiresGtk2(pluginPath))
        return false;
#endif

    CString binaryPath = FileSystem::fileSystemRepresentation(pluginProcessPath);
    CString pluginPathCString = FileSystem::fileSystemRepresentation(pluginPath);
    char* argv[4];
    argv[0] = const_cast<char*>(binaryPath.data());
    argv[1] = const_cast<char*>("-scanPlugin");
    argv[2] = const_cast<char*>(pluginPathCString.data());
    argv[3] = nullptr;

    // If the disposition of SIGCLD signal is set to SIG_IGN (default)
    // then the signal will be ignored and g_spawn_sync() will not be
    // able to return the status.
    // As a consequence, we make sure that the disposition is set to
    // SIG_DFL before calling g_spawn_sync().
#if defined(SIGCLD)
    struct sigaction action;
    sigaction(SIGCLD, 0, &action);
    if (action.sa_handler == SIG_IGN) {
        action.sa_handler = SIG_DFL;
        sigaction(SIGCLD, &action, 0);
    }
#endif

    int status;
    GUniqueOutPtr<char> stdOut;
    GUniqueOutPtr<GError> error;
    if (!g_spawn_sync(nullptr, argv, nullptr, G_SPAWN_STDERR_TO_DEV_NULL, nullptr, nullptr, &stdOut.outPtr(), nullptr, &status, &error.outPtr())) {
        WTFLogAlways("Failed to launch %s: %s", argv[0], error->message);
        return false;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
        WTFLogAlways("Error scanning plugin %s, %s returned %d exit status", argv[2], argv[0], status);
        return false;
    }

    if (!stdOut) {
        WTFLogAlways("Error scanning plugin %s, %s didn't write any output to stdout", argv[2], argv[0]);
        return false;
    }

    Vector<String> lines = String::fromUTF8(stdOut.get()).splitAllowingEmptyEntries('\n');

    if (lines.size() < 3) {
        WTFLogAlways("Error scanning plugin %s, too few lines of output provided", argv[2]);
        return false;
    }

    result.name.swap(lines[0]);
    result.description.swap(lines[1]);
    result.mimeDescription.swap(lines[2]);
    return !result.mimeDescription.isEmpty();
}
#endif // PLUGIN_ARCHITECTURE(UNIX)

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)
