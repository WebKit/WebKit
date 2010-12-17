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

#include "ProcessLauncher.h"

#include "RunLoop.h"
#include "WebProcess.h"
#include "WebKitSystemInterface.h"
#include <crt_externs.h>
#include <mach-o/dyld.h>
#include <mach/machine.h>
#include <runtime/InitializeThreading.h>
#include <servers/bootstrap.h>
#include <spawn.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/Threading.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

// FIXME: We should be doing this another way.
extern "C" kern_return_t bootstrap_register2(mach_port_t, name_t, mach_port_t, uint64_t);

namespace WebKit {

#if !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
static const char* processName()
{
    return [[[NSProcessInfo processInfo] processName] fileSystemRepresentation];
}
#else
// -[NSProcessInfo processName] isn't thread-safe on Leopard and Snow Leopard so we have our own implementation.
static const char* createProcessName()
{
    uint32_t bufferSize = MAXPATHLEN;
    char executablePath[bufferSize];
    
    if (_NSGetExecutablePath(executablePath, &bufferSize))
        return "";
    
    const char *processName = strrchr(executablePath, '/') + 1;
    return strdup(processName);
}

static const char* processName()
{
    static const char* processName = createProcessName();
    return processName;
}
#endif

static void setUpTerminationNotificationHandler(pid_t pid)
{
#if HAVE(DISPATCH_H)
    dispatch_source_t processDiedSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, pid, DISPATCH_PROC_EXIT, dispatch_get_current_queue());
    dispatch_source_set_event_handler(processDiedSource, ^{
        int status;
        waitpid(dispatch_source_get_handle(processDiedSource), &status, 0);
        dispatch_source_cancel(processDiedSource);
    });
    dispatch_source_set_cancel_handler(processDiedSource, ^{
        dispatch_release(processDiedSource);
    });
    dispatch_resume(processDiedSource);
#endif
}

class EnvironmentVariables {
    WTF_MAKE_NONCOPYABLE(EnvironmentVariables);

public:
    EnvironmentVariables()
        : m_environmentPointer(*_NSGetEnviron())
    {
    }

    ~EnvironmentVariables()
    {
        deleteAllValues(m_allocatedStrings);
    }

    void set(const char* name, const char* value)
    {
        // Check if we need to copy the environment.
        if (m_environmentPointer == *_NSGetEnviron())
            copyEnvironmentVariables();

        // Allocate a string for the name and value.
        char* nameAndValue = createStringForVariable(name, value);

        for (size_t i = 0; i < m_environmentVariables.size() - 1; ++i) {
            char* environmentVariable = m_environmentVariables[i];

            if (valueIfVariableHasName(environmentVariable, name)) {
                // Just replace the environment variable.
                m_environmentVariables[i] = nameAndValue;
                return;
            }
        }

        // Append the new string.
        ASSERT(!m_environmentVariables.last());
        m_environmentVariables.last() = nameAndValue;
        m_environmentVariables.append(static_cast<char*>(0));

        m_environmentPointer = m_environmentVariables.data();
    }

    char* get(const char* name) const
    {
        for (size_t i = 0; m_environmentPointer[i]; ++i) {
            if (char* value = valueIfVariableHasName(m_environmentPointer[i], name))
                return value;
        }
        return 0;
    }

    // Will append the value with the given separator if the environment variable already exists.
    void appendValue(const char* name, const char* value, char separator)
    {
        char* existingValue = get(name);
        if (!existingValue) {
            set(name, value);
            return;
        }

        Vector<char, 128> newValue;
        newValue.append(existingValue, strlen(existingValue));
        newValue.append(separator);
        newValue.append(value, strlen(value) + 1);

        set(name, newValue.data());
    }

    char** environmentPointer() const { return m_environmentPointer; }

private:
    char *valueIfVariableHasName(const char* environmentVariable, const char* name) const
    {
        // Find the environment variable name.
        char* equalsLocation = strchr(environmentVariable, '=');
        ASSERT(equalsLocation);

        size_t nameLength = equalsLocation - environmentVariable;
        if (strncmp(environmentVariable, name, nameLength))
            return 0;

        return equalsLocation + 1;
    }

    char* createStringForVariable(const char* name, const char* value)
    {
        int nameLength = strlen(name);
        int valueLength = strlen(value);

        // Allocate enough room to hold 'name=value' and the null character.
        char* string = static_cast<char*>(fastMalloc(nameLength + 1 + valueLength + 1));
        memcpy(string, name, nameLength);
        string[nameLength] = '=';
        memcpy(string + nameLength + 1, value, valueLength);
        string[nameLength + 1 + valueLength] = '\0';

        m_allocatedStrings.append(string);

        return string;
    }

    void copyEnvironmentVariables()
    {
        for (size_t i = 0; (*_NSGetEnviron())[i]; i++)
            m_environmentVariables.append((*_NSGetEnviron())[i]);

        // Null-terminate the array.
        m_environmentVariables.append(static_cast<char*>(0));
    }

    char** m_environmentPointer;
    Vector<char*> m_environmentVariables;

    // These allocated strings will be freed in the destructor.
    Vector<char*> m_allocatedStrings;
};

void ProcessLauncher::launchProcess()
{
    // Create the listening port.
    mach_port_t listeningPort;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);
    
    // Insert a send right so we can send to it.
    mach_port_insert_right(mach_task_self(), listeningPort, listeningPort, MACH_MSG_TYPE_MAKE_SEND);

    NSBundle *webKit2Bundle = [NSBundle bundleWithIdentifier:@"com.apple.WebKit2"];
    const char* bundlePath = [[webKit2Bundle executablePath] fileSystemRepresentation];

    NSString *webProcessAppPath = [webKit2Bundle pathForAuxiliaryExecutable:@"WebProcess.app"];
    NSString *webProcessAppExecutablePath = [[NSBundle bundleWithPath:webProcessAppPath] executablePath];

    // Make a unique, per pid, per process launcher web process service name.
    CString serviceName = String::format("com.apple.WebKit.WebProcess-%d-%p", getpid(), this).utf8();

    const char* path = [webProcessAppExecutablePath fileSystemRepresentation];
    const char* args[] = { path, bundlePath, "-type", processTypeAsString(m_launchOptions.processType), "-servicename", serviceName.data(), "-parentprocessname", processName(), 0 };

    // Register ourselves.
    kern_return_t kr = bootstrap_register2(bootstrap_port, const_cast<char*>(serviceName.data()), listeningPort, 0);
    ASSERT_UNUSED(kr, kr == KERN_SUCCESS);

    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);

    short flags = 0;

    // We want our process to receive all signals.
    sigset_t signalMaskSet;
    sigemptyset(&signalMaskSet);

    posix_spawnattr_setsigmask(&attr, &signalMaskSet);
    flags |= POSIX_SPAWN_SETSIGMASK;

    // Determine the architecture to use.
    cpu_type_t architecture = m_launchOptions.architecture;
    if (architecture == LaunchOptions::MatchCurrentArchitecture)
        architecture = _NSGetMachExecuteHeader()->cputype;

    cpu_type_t cpuTypes[] = { architecture };    
    size_t outCount = 0;
    posix_spawnattr_setbinpref_np(&attr, 1, cpuTypes, &outCount);

    // Start suspended so we can set up the termination notification handler.
    flags |= POSIX_SPAWN_START_SUSPENDED;

    posix_spawnattr_setflags(&attr, flags);

    pid_t processIdentifier;

    EnvironmentVariables environmentVariables;

    if (m_launchOptions.processType == ProcessLauncher::PluginProcess) {
        // We need to insert the plug-in process shim.
        NSString *pluginProcessShimPathNSString = [[webProcessAppExecutablePath stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"PluginProcessShim.dylib"];
        const char *pluginProcessShimPath = [pluginProcessShimPathNSString fileSystemRepresentation];

        // Make sure that the file exists.
        struct stat statBuf;
        if (stat(pluginProcessShimPath, &statBuf) == 0 && (statBuf.st_mode & S_IFMT) == S_IFREG)
            environmentVariables.appendValue("DYLD_INSERT_LIBRARIES", pluginProcessShimPath, ':');
    }
    
    int result = posix_spawn(&processIdentifier, path, 0, &attr, (char *const*)args, environmentVariables.environmentPointer());

    posix_spawnattr_destroy(&attr);

    if (!result) {
        // Set up the termination notification handler and then ask the child process to continue.
        setUpTerminationNotificationHandler(processIdentifier);
        kill(processIdentifier, SIGCONT);
    } else {
        // We failed to launch. Release the send right.
        mach_port_deallocate(mach_task_self(), listeningPort);

        // And the receive right.
        mach_port_mod_refs(mach_task_self(), listeningPort, MACH_PORT_RIGHT_RECEIVE, -1);
        
        listeningPort = MACH_PORT_NULL;
        processIdentifier = 0;
    }
    
    // We've finished launching the process, message back to the main run loop.
    RunLoop::main()->scheduleWork(WorkItem::create(this, &ProcessLauncher::didFinishLaunchingProcess, processIdentifier, listeningPort));
}

void ProcessLauncher::terminateProcess()
{    
    if (!m_processIdentifier)
        return;
    
    kill(m_processIdentifier, SIGKILL);
}
    
void ProcessLauncher::platformInvalidate()
{
}

} // namespace WebKit
