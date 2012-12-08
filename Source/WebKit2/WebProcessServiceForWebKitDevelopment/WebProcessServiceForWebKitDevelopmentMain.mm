/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#import <AvailabilityMacros.h>

#import <crt_externs.h>
#import <dlfcn.h>
#import <mach-o/dyld.h>
#import <spawn.h> 
#import <stdio.h>
#import <stdlib.h>
#import <xpc/xpc.h>

extern "C" mach_port_t xpc_dictionary_copy_mach_send(xpc_object_t, const char*);

static void WebProcessServiceForWebKitDevelopmentEventHandler(xpc_connection_t peer)
{
    xpc_connection_set_target_queue(peer, dispatch_get_main_queue());
    xpc_connection_set_event_handler(peer, ^(xpc_object_t event) {
        xpc_type_t type = xpc_get_type(event);
        if (type == XPC_TYPE_ERROR) {
            if (event == XPC_ERROR_CONNECTION_INVALID || event == XPC_ERROR_TERMINATION_IMMINENT) {
                // FIXME: Handle this case more gracefully.
                exit(EXIT_FAILURE);
            }
        } else {
            assert(type == XPC_TYPE_DICTIONARY);

            if (!strcmp(xpc_dictionary_get_string(event, "message-name"), "re-exec")) {
                // Setup the posix_spawn attributes.
                posix_spawnattr_t attr;
                posix_spawnattr_init(&attr);

                short flags = 0;

                // We just want to set the process state, not actually launch a new process,
                // so we are going to use the darwin extension to posix_spawn POSIX_SPAWN_SETEXEC
                // to act like a more full featured exec.
                flags |= POSIX_SPAWN_SETEXEC;

                // We want our process to receive all signals.
                sigset_t signalMaskSet;
                sigemptyset(&signalMaskSet);
                posix_spawnattr_setsigmask(&attr, &signalMaskSet);
                flags |= POSIX_SPAWN_SETSIGMASK;

                // Set the architecture.
                cpu_type_t cpuTypes[] = { (cpu_type_t)xpc_dictionary_get_uint64(event, "architecture") };
                size_t outCount = 0;
                posix_spawnattr_setbinpref_np(&attr, 1, cpuTypes, &outCount);

                static const int allowExecutableHeapFlag = 0x2000;
                if (xpc_dictionary_get_bool(event, "executable-heap"))
                    flags |= allowExecutableHeapFlag;

                posix_spawnattr_setflags(&attr, flags);

                char path[4 * PATH_MAX];
                uint32_t pathLength = sizeof(path);
                _NSGetExecutablePath(path, &pathLength);

                // Setup the command line.
                char** argv = *_NSGetArgv();
                const char* programName = argv[0];
                const char* args[] = { programName, 0 };

                // Setup the environment.
                xpc_object_t environmentArray = xpc_dictionary_get_value(event, "environment");
                size_t numberOfEnvironmentVariables = xpc_array_get_count(environmentArray);

                char** environment = (char**)malloc(numberOfEnvironmentVariables * sizeof(char*) + 1);
                for (size_t i = 0; i < numberOfEnvironmentVariables; ++i) {
                    const char* string =  xpc_array_get_string(environmentArray, i);
                    size_t stringLength = strlen(string);

                    char* environmentVariable = (char*)malloc(stringLength + 1);
                    memcpy(environmentVariable, string, stringLength);
                    environmentVariable[stringLength] = '\0';

                    environment[i] = environmentVariable;
                }
                environment[numberOfEnvironmentVariables] = 0;

                pid_t processIdentifier = 0;
                posix_spawn(&processIdentifier, path, 0, &attr, const_cast<char**>(args), environment);

                posix_spawnattr_destroy(&attr);

                NSLog(@"Unable to re-exec for path: %s\n", path);
                exit(EXIT_FAILURE);
            }

            if (!strcmp(xpc_dictionary_get_string(event, "message-name"), "bootstrap")) {
                static void* frameworkLibrary = dlopen(xpc_dictionary_get_string(event, "framework-executable-path"), RTLD_NOW);
                if (!frameworkLibrary) {
                    NSLog(@"Unable to load WebKit2.framework: %s\n", dlerror());
                    exit(EXIT_FAILURE);
                }

                typedef void (*InitializeWebProcessFunction)(const char* clientIdentifer, xpc_connection_t connection, mach_port_t serverPort, const char* uiProcessName);
                InitializeWebProcessFunction initializeWebProcessFunctionPtr = reinterpret_cast<InitializeWebProcessFunction>(dlsym(frameworkLibrary, "initializeWebProcessForWebProcessServiceForWebKitDevelopment"));
                if (!initializeWebProcessFunctionPtr) {
                    NSLog(@"Unable to find entry point in WebKit2.framework: %s\n", dlerror());
                    exit(EXIT_FAILURE);
                }

                xpc_object_t reply = xpc_dictionary_create_reply(event);
                xpc_dictionary_set_string(reply, "message-name", "process-finished-launching");
                xpc_connection_send_message(xpc_dictionary_get_remote_connection(event), reply);
                xpc_release(reply);

                dup2(xpc_dictionary_dup_fd(event, "stdout"), STDOUT_FILENO);
                dup2(xpc_dictionary_dup_fd(event, "stderr"), STDERR_FILENO);

                initializeWebProcessFunctionPtr(xpc_dictionary_get_string(event, "client-identifier"), peer, xpc_dictionary_copy_mach_send(event, "server-port"), xpc_dictionary_get_string(event, "ui-process-name"));
            }
        }
    });

    xpc_connection_resume(peer);
}

int main(int argc, char** argv)
{
    xpc_main(WebProcessServiceForWebKitDevelopmentEventHandler);
    return 0;;
}
