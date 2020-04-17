//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// system_utils_posix.cpp: Implementation of POSIX OS-specific functions.

#include "system_utils.h"

#include <array>

#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace angle
{
Optional<std::string> GetCWD()
{
    std::array<char, 4096> pathBuf;
    char *result = getcwd(pathBuf.data(), pathBuf.size());
    if (result == nullptr)
    {
        return Optional<std::string>::Invalid();
    }
    return std::string(pathBuf.data());
}

bool SetCWD(const char *dirName)
{
    return (chdir(dirName) == 0);
}

bool UnsetEnvironmentVar(const char *variableName)
{
    return (unsetenv(variableName) == 0);
}

bool SetEnvironmentVar(const char *variableName, const char *value)
{
    return (setenv(variableName, value, 1) == 0);
}

std::string GetEnvironmentVar(const char *variableName)
{
    const char *value = getenv(variableName);
    return (value == nullptr ? std::string() : std::string(value));
}

const char *GetPathSeparatorForEnvironmentVar()
{
    return ":";
}

class PosixLibrary : public Library
{
  public:
    PosixLibrary(const char *libraryName, SearchType searchType)
    {
        std::string directory;
        if (searchType == SearchType::ApplicationDir)
        {
            static int dummySymbol = 0;
            Dl_info dlInfo;
            if (dladdr(&dummySymbol, &dlInfo) != 0)
            {
                std::string moduleName = dlInfo.dli_fname;
                directory              = moduleName.substr(0, moduleName.find_last_of('/') + 1);
            }
        }

        std::string fullPath = directory + libraryName + "." + GetSharedLibraryExtension();
        mModule              = dlopen(fullPath.c_str(), RTLD_NOW);
    }

    ~PosixLibrary() override
    {
        if (mModule)
        {
            dlclose(mModule);
        }
    }

    void *getSymbol(const char *symbolName) override
    {
        if (!mModule)
        {
            return nullptr;
        }

        return dlsym(mModule, symbolName);
    }

    void *getNative() const override { return mModule; }

  private:
    void *mModule = nullptr;
};

Library *OpenSharedLibrary(const char *libraryName, SearchType searchType)
{
    return new PosixLibrary(libraryName, searchType);
}

bool IsDirectory(const char *filename)
{
    struct stat st;
    int result = stat(filename, &st);
    return result == 0 && ((st.st_mode & S_IFDIR) == S_IFDIR);
}

bool IsDebuggerAttached()
{
    // This could have a fuller implementation.
    // See https://cs.chromium.org/chromium/src/base/debug/debugger_posix.cc
    return false;
}

void BreakDebugger()
{
    // This could have a fuller implementation.
    // See https://cs.chromium.org/chromium/src/base/debug/debugger_posix.cc
    abort();
}

const char *GetExecutableExtension()
{
    return "";
}

char GetPathSeparator()
{
    return '/';
}
}  // namespace angle
