//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Win32_system_utils.cpp: Implementation of OS-specific functions for Win32 (Windows)

#include "system_utils.h"

#include <windows.h>
#include <array>

namespace angle
{

void SetLowPriorityProcess()
{
    SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
}

class Win32Library : public Library
{
  public:
    Win32Library(const std::string &libraryName);
    ~Win32Library() override;

    void *getSymbol(const std::string &symbolName) override;

  private:
    HMODULE mModule;
};

Win32Library::Win32Library(const std::string &libraryName) : mModule(nullptr)
{
    const auto &fullName = libraryName + "." + GetSharedLibraryExtension();
    mModule              = LoadLibraryA(fullName.c_str());
}

Win32Library::~Win32Library()
{
    if (mModule)
    {
        FreeLibrary(mModule);
    }
}

void *Win32Library::getSymbol(const std::string &symbolName)
{
    if (!mModule)
    {
        return nullptr;
    }

    return GetProcAddress(mModule, symbolName.c_str());
}

Library *loadLibrary(const std::string &libraryName)
{
    return new Win32Library(libraryName);
}

}  // namespace angle
