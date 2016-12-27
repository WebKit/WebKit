//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Posix_system_utils.cpp: Implementation of OS-specific functions for Posix systems

#include "system_utils.h"

#include <sys/resource.h>
#include <dlfcn.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

namespace angle
{

void Sleep(unsigned int milliseconds)
{
    // On Windows Sleep(0) yields while it isn't guaranteed by Posix's sleep
    // so we replicate Windows' behavior with an explicit yield.
    if (milliseconds == 0)
    {
        sched_yield();
    }
    else
    {
        timespec sleepTime =
        {
            .tv_sec = milliseconds / 1000,
            .tv_nsec = (milliseconds % 1000) * 1000000,
        };

        nanosleep(&sleepTime, nullptr);
    }
}

void SetLowPriorityProcess()
{
    setpriority(PRIO_PROCESS, getpid(), 10);
}

void WriteDebugMessage(const char *format, ...)
{
    // TODO(jmadill): Implement this
}

class PosixLibrary : public Library
{
  public:
    PosixLibrary(const std::string &libraryName);
    ~PosixLibrary() override;

    void *getSymbol(const std::string &symbolName) override;

  private:
    void *mModule;
};

PosixLibrary::PosixLibrary(const std::string &libraryName) : mModule(nullptr)
{
    const auto &fullName = libraryName + "." + GetSharedLibraryExtension();
    mModule              = dlopen(fullName.c_str(), RTLD_NOW);
}

PosixLibrary::~PosixLibrary()
{
    if (mModule)
    {
        dlclose(mModule);
    }
}

void *PosixLibrary::getSymbol(const std::string &symbolName)
{
    if (!mModule)
    {
        return nullptr;
    }

    return dlsym(mModule, symbolName.c_str());
}

Library *loadLibrary(const std::string &libraryName)
{
    return new PosixLibrary(libraryName);
}

} // namespace angle
