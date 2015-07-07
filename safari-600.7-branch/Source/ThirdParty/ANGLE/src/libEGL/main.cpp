//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// main.cpp: DLL entry point and management of thread-local data.

#include "libEGL/main.h"

#include "common/debug.h"

static DWORD currentTLS = TLS_OUT_OF_INDEXES;

namespace egl
{

Current *AllocateCurrent()
{
    Current *current = (egl::Current*)LocalAlloc(LPTR, sizeof(egl::Current));

    if (!current)
    {
        ERR("Could not allocate thread local storage.");
        return NULL;
    }

    ASSERT(currentTLS != TLS_OUT_OF_INDEXES);
    TlsSetValue(currentTLS, current);

    current->error = EGL_SUCCESS;
    current->API = EGL_OPENGL_ES_API;
    current->display = EGL_NO_DISPLAY;
    current->drawSurface = EGL_NO_SURFACE;
    current->readSurface = EGL_NO_SURFACE;

    return current;
}

void DeallocateCurrent()
{
    void *current = TlsGetValue(currentTLS);

    if (current)
    {
        LocalFree((HLOCAL)current);
    }
}

}

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
      case DLL_PROCESS_ATTACH:
        {
#if defined(ANGLE_ENABLE_TRACE)
            FILE *debug = fopen(TRACE_OUTPUT_FILE, "rt");

            if (debug)
            {
                fclose(debug);
                debug = fopen(TRACE_OUTPUT_FILE, "wt");   // Erase
                
                if (debug)
                {
                    fclose(debug);
                }
            }
#endif

            currentTLS = TlsAlloc();

            if (currentTLS == TLS_OUT_OF_INDEXES)
            {
                return FALSE;
            }
        }
        // Fall throught to initialize index
      case DLL_THREAD_ATTACH:
        {
            egl::AllocateCurrent();
        }
        break;
      case DLL_THREAD_DETACH:
        {
            egl::DeallocateCurrent();
        }
        break;
      case DLL_PROCESS_DETACH:
        {
            egl::DeallocateCurrent();
            TlsFree(currentTLS);
        }
        break;
      default:
        break;
    }

    return TRUE;
}

namespace egl
{

Current *GetCurrentData()
{
    Current *current = (Current*)TlsGetValue(currentTLS);

    // ANGLE issue 488: when the dll is loaded after thread initialization,
    // thread local storage (current) might not exist yet.
    return (current ? current : AllocateCurrent());
}

void setCurrentError(EGLint error)
{
    Current *current = GetCurrentData();

    current->error = error;
}

EGLint getCurrentError()
{
    Current *current = GetCurrentData();

    return current->error;
}

void setCurrentAPI(EGLenum API)
{
    Current *current = GetCurrentData();

    current->API = API;
}

EGLenum getCurrentAPI()
{
    Current *current = GetCurrentData();

    return current->API;
}

void setCurrentDisplay(EGLDisplay dpy)
{
    Current *current = GetCurrentData();

    current->display = dpy;
}

EGLDisplay getCurrentDisplay()
{
    Current *current = GetCurrentData();

    return current->display;
}

void setCurrentDrawSurface(EGLSurface surface)
{
    Current *current = GetCurrentData();

    current->drawSurface = surface;
}

EGLSurface getCurrentDrawSurface()
{
    Current *current = GetCurrentData();

    return current->drawSurface;
}

void setCurrentReadSurface(EGLSurface surface)
{
    Current *current = GetCurrentData();

    current->readSurface = surface;
}

EGLSurface getCurrentReadSurface()
{
    Current *current = GetCurrentData();

    return current->readSurface;
}

void error(EGLint errorCode)
{
    egl::setCurrentError(errorCode);
}

}
