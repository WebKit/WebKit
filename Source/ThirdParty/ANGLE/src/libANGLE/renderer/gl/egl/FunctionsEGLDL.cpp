//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FunctionsEGLDL.cpp: Implements the FunctionsEGLDL class.

#include "libANGLE/renderer/gl/egl/FunctionsEGLDL.h"

#include <dlfcn.h>

namespace rx
{

DynamicLib::DynamicLib() : handle(nullptr)
{
}

DynamicLib::~DynamicLib()
{
    if (handle)
    {
        dlclose(handle);
        handle = nullptr;
    }
}

// Due to a bug in Mesa (or maybe libdl) it's not possible to close and re-open libEGL.so
// an arbitrary number of times.  End2end tests would die after a couple hundred tests.
// So we use a static object with a destructor to close the library when the program exits.
// TODO(fjhenigman) File a bug and put a link here.
DynamicLib FunctionsEGLDL::sNativeLib;

FunctionsEGLDL::FunctionsEGLDL() : mGetProcAddressPtr(nullptr)
{
}

FunctionsEGLDL::~FunctionsEGLDL()
{
}

egl::Error FunctionsEGLDL::initialize(EGLNativeDisplayType nativeDisplay, const char *libName)
{
    if (!sNativeLib.handle)
    {
        sNativeLib.handle = dlopen(libName, RTLD_NOW);
        if (!sNativeLib.handle)
        {
            return egl::Error(EGL_NOT_INITIALIZED, "Could not dlopen native EGL: %s", dlerror());
        }
    }

    mGetProcAddressPtr =
        reinterpret_cast<PFNEGLGETPROCADDRESSPROC>(dlsym(sNativeLib.handle, "eglGetProcAddress"));
    if (!mGetProcAddressPtr)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Could not find eglGetProcAddress");
    }

    return FunctionsEGL::initialize(nativeDisplay);
}

void *FunctionsEGLDL::getProcAddress(const char *name) const
{
    void *f = reinterpret_cast<void *>(mGetProcAddressPtr(name));
    if (f)
    {
        return f;
    }
    return dlsym(sNativeLib.handle, name);
}

}  // namespace rx
