//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// main.cpp: DLL entry point and management of thread-local data.

#include "libGLESv2/main.h"

#include "common/debug.h"
#include "libEGL/Surface.h"

#include "libGLESv2/Framebuffer.h"

static DWORD currentTLS = TLS_OUT_OF_INDEXES;

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
      case DLL_PROCESS_ATTACH:
        {
            currentTLS = TlsAlloc();

            if (currentTLS == TLS_OUT_OF_INDEXES)
            {
                return FALSE;
            }
        }
        // Fall throught to initialize index
      case DLL_THREAD_ATTACH:
        {
            gl::Current *current = (gl::Current*)LocalAlloc(LPTR, sizeof(gl::Current));

            if (current)
            {
                TlsSetValue(currentTLS, current);

                current->context = NULL;
                current->display = NULL;
            }
        }
        break;
      case DLL_THREAD_DETACH:
        {
            void *current = TlsGetValue(currentTLS);

            if (current)
            {
                LocalFree((HLOCAL)current);
            }
        }
        break;
      case DLL_PROCESS_DETACH:
        {
            void *current = TlsGetValue(currentTLS);

            if (current)
            {
                LocalFree((HLOCAL)current);
            }

            TlsFree(currentTLS);
        }
        break;
      default:
        break;
    }

    return TRUE;
}

namespace gl
{
void makeCurrent(Context *context, egl::Display *display, egl::Surface *surface)
{
    Current *current = (Current*)TlsGetValue(currentTLS);

    current->context = context;
    current->display = display;

    if (context && display && surface)
    {
        context->makeCurrent(display, surface);
    }
}

Context *getContext()
{
    Current *current = (Current*)TlsGetValue(currentTLS);

    return current->context;
}

egl::Display *getDisplay()
{
    Current *current = (Current*)TlsGetValue(currentTLS);

    return current->display;
}

IDirect3DDevice9 *getDevice()
{
    egl::Display *display = getDisplay();

    return display->getDevice();
}
}

// Records an error code
void error(GLenum errorCode)
{
    gl::Context *context = glGetCurrentContext();

    if (context)
    {
        switch (errorCode)
        {
          case GL_INVALID_ENUM:
            context->recordInvalidEnum();
            gl::trace("\t! Error generated: invalid enum\n");
            break;
          case GL_INVALID_VALUE:
            context->recordInvalidValue();
            gl::trace("\t! Error generated: invalid value\n");
            break;
          case GL_INVALID_OPERATION:
            context->recordInvalidOperation();
            gl::trace("\t! Error generated: invalid operation\n");
            break;
          case GL_OUT_OF_MEMORY:
            context->recordOutOfMemory();
            gl::trace("\t! Error generated: out of memory\n");
            break;
          case GL_INVALID_FRAMEBUFFER_OPERATION:
            context->recordInvalidFramebufferOperation();
            gl::trace("\t! Error generated: invalid framebuffer operation\n");
            break;
          default: UNREACHABLE();
        }
    }
}
