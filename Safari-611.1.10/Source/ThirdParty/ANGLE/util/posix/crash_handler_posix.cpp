//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// crash_handler_posix:
//    ANGLE's crash handling and stack walking code. Modified from Skia's:
//     https://github.com/google/skia/blob/master/tools/CrashHandler.cpp
//

#include "util/test_utils.h"

#include "common/angleutils.h"
#include "common/system_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>

#if !defined(ANGLE_PLATFORM_ANDROID) && !defined(ANGLE_PLATFORM_FUCHSIA)
#    if defined(ANGLE_PLATFORM_APPLE)
// We only use local unwinding, so we can define this to select a faster implementation.
#        define UNW_LOCAL_ONLY
#        include <cxxabi.h>
#        include <libunwind.h>
#        include <signal.h>
#    elif defined(ANGLE_PLATFORM_POSIX)
// We'd use libunwind here too, but it's a pain to get installed for
// both 32 and 64 bit on bots.  Doesn't matter much: catchsegv is best anyway.
#        include <cxxabi.h>
#        include <dlfcn.h>
#        include <execinfo.h>
#        include <libgen.h>
#        include <signal.h>
#        include <string.h>
#    endif  // defined(ANGLE_PLATFORM_APPLE)
#endif      // !defined(ANGLE_PLATFORM_ANDROID) && !defined(ANGLE_PLATFORM_FUCHSIA)

namespace angle
{
#if defined(ANGLE_PLATFORM_ANDROID) || defined(ANGLE_PLATFORM_FUCHSIA)

void PrintStackBacktrace()
{
    // No implementations yet.
}

void InitCrashHandler(CrashCallback *callback)
{
    // No implementations yet.
}

void TerminateCrashHandler()
{
    // No implementations yet.
}

#else
namespace
{
CrashCallback *gCrashHandlerCallback;
}  // namespace

#    if defined(ANGLE_PLATFORM_APPLE)

void PrintStackBacktrace()
{
    printf("Backtrace:\n");

    unw_context_t context;
    unw_getcontext(&context);

    unw_cursor_t cursor;
    unw_init_local(&cursor, &context);

    while (unw_step(&cursor) > 0)
    {
        static const size_t kMax = 256;
        char mangled[kMax], demangled[kMax];
        unw_word_t offset;
        unw_get_proc_name(&cursor, mangled, kMax, &offset);

        int ok;
        size_t len = kMax;
        abi::__cxa_demangle(mangled, demangled, &len, &ok);

        printf("    %s (+0x%zx)\n", ok == 0 ? demangled : mangled, (size_t)offset);
    }
    printf("\n");
}

static void Handler(int sig)
{
    if (gCrashHandlerCallback)
    {
        (*gCrashHandlerCallback)();
    }

    printf("\nSignal %d:\n", sig);
    PrintStackBacktrace();

    // Exit NOW.  Don't notify other threads, don't call anything registered with atexit().
    _Exit(sig);
}

#    elif defined(ANGLE_PLATFORM_POSIX)

// Can control this at a higher level if required.
#        define ANGLE_HAS_ADDR2LINE

void PrintStackBacktrace()
{
    printf("Backtrace:\n");

    void *stack[64];
    const int count = backtrace(stack, ArraySize(stack));
    char **symbols  = backtrace_symbols(stack, count);

    for (int i = 0; i < count; i++)
    {
#        if defined(ANGLE_HAS_ADDR2LINE)
        std::string module(symbols[i], strchr(symbols[i], '('));

        // We need an absolute path to get to the executable and all of the various shared objects,
        // but the caller may have used a relative path to launch the executable, so build one up if
        // we don't see a leading '/'.
        if (module.at(0) != GetPathSeparator())
        {
            const Optional<std::string> &cwd = angle::GetCWD();
            if (!cwd.valid())
            {
                std::cout << "Error getting CWD for Vulkan layers init." << std::endl;
            }
            else
            {
                std::string absolutePath = cwd.value();
                size_t lastPathSepLoc    = module.find_last_of(GetPathSeparator());
                std::string relativePath = module.substr(0, lastPathSepLoc);

                // Remove "." from the relativePath path
                // For example: ./out/LinuxDebug/angle_perftests
                size_t pos = relativePath.find('.');
                if (pos != std::string::npos)
                {
                    // If found then erase it from string
                    relativePath.erase(pos, 1);
                }

                // Remove the overlapping relative path from the CWD so we can build the full
                // absolute path.
                // For example:
                // absolutePath = /home/timvp/code/angle/out/LinuxDebug
                // relativePath = /out/LinuxDebug
                pos = absolutePath.find(relativePath);
                if (pos != std::string::npos)
                {
                    // If found then erase it from string
                    absolutePath.erase(pos, relativePath.length());
                }
                module = absolutePath + GetPathSeparator() + module;
            }
        }

        std::string substring(strchr(symbols[i], '+') + 1, strchr(symbols[i], ')'));

        pid_t pid = fork();
        if (pid < 0)
        {
            std::cout << "Error: Failed to fork()";
        }
        else if (pid > 0)
        {
            int status;
            waitpid(pid, &status, 0);
            // Ignore the status, since we aren't going to handle it anyway.
        }
        else
        {
            // Child process executes addr2line
            const std::vector<const char *> &commandLineArgs = {
                "/usr/bin/addr2line",  // execv requires an absolute path to find addr2line
                "-s",
                "-p",
                "-f",
                "-C",
                "-e",
                module.c_str(),
                substring.c_str()};
            // Taken from test_utils_posix.cpp::PosixProcess
            std::vector<char *> argv;
            for (const char *arg : commandLineArgs)
            {
                argv.push_back(const_cast<char *>(arg));
            }
            argv.push_back(nullptr);
            execv(argv[0], argv.data());
            std::cout << "Error: Child process returned from exevc()";
            _exit(EXIT_FAILURE);  // exec never returns
        }
#        else
        Dl_info info;
        if (dladdr(stack[i], &info) && info.dli_sname)
        {
            // Make sure this is large enough to hold the fully demangled names, otherwise we could
            // segault/hang here. For example, Vulkan validation layer errors can be deep enough
            // into the stack that very large symbol names are generated.
            char demangled[4096];
            size_t len = ArraySize(demangled);
            int ok;

            abi::__cxa_demangle(info.dli_sname, demangled, &len, &ok);
            if (ok == 0)
            {
                printf("    %s\n", demangled);
                continue;
            }
        }
        printf("    %s\n", symbols[i]);
#        endif  // defined(ANGLE_HAS_ADDR2LINE)
    }
}

static void Handler(int sig)
{
    if (gCrashHandlerCallback)
    {
        (*gCrashHandlerCallback)();
    }

    printf("\nSignal %d [%s]:\n", sig, strsignal(sig));
    PrintStackBacktrace();

    // Exit NOW.  Don't notify other threads, don't call anything registered with atexit().
    _Exit(sig);
}

#    endif  // defined(ANGLE_PLATFORM_APPLE)

static constexpr int kSignals[] = {
    SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSEGV, SIGTRAP,
};

void InitCrashHandler(CrashCallback *callback)
{
    gCrashHandlerCallback = callback;
    for (int sig : kSignals)
    {
        // Register our signal handler unless something's already done so (e.g. catchsegv).
        void (*prev)(int) = signal(sig, Handler);
        if (prev != SIG_DFL)
        {
            signal(sig, prev);
        }
    }
}

void TerminateCrashHandler()
{
    gCrashHandlerCallback = nullptr;
    for (int sig : kSignals)
    {
        void (*prev)(int) = signal(sig, SIG_DFL);
        if (prev != Handler && prev != SIG_DFL)
        {
            signal(sig, prev);
        }
    }
}

#endif  // defined(ANGLE_PLATFORM_ANDROID) || defined(ANGLE_PLATFORM_FUCHSIA)

}  // namespace angle
