/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GraphicsContextGLANGLEEGLUtilities.h"

#include "ANGLEHeaders.h"
#include <wtf/Assertions.h>

#if ENABLE(WEBGL) && USE(ANGLE)

namespace WebCore {

static unsigned defaultDisplayRefCount;
static void* defaultDisplay = nullptr;

static void refDefaultDisplay(void* display)
{
    ASSERT(display != nullptr);
    ASSERT(defaultDisplay == display || (!defaultDisplay && !defaultDisplayRefCount));
    defaultDisplayRefCount++;
    defaultDisplay = display;
}

static void unrefDefaultDisplayIfNeeded(void* display)
{
    if (!display)
        return;
    ASSERT(defaultDisplay == display);
    ASSERT(defaultDisplayRefCount);
    defaultDisplayRefCount--;
}

ScopedEGLDefaultDisplay& ScopedEGLDefaultDisplay::operator=(ScopedEGLDefaultDisplay&& other)
{
    if (this != &other) {
        unrefDefaultDisplayIfNeeded(m_display);
        m_display = std::exchange(other.m_display, nullptr);
    }
    return *this;
}

ScopedEGLDefaultDisplay::ScopedEGLDefaultDisplay(void* display)
    : m_display(display)
{
    refDefaultDisplay(m_display);
}

ScopedEGLDefaultDisplay::~ScopedEGLDefaultDisplay()
{
    unrefDefaultDisplayIfNeeded(m_display);
}

void ScopedEGLDefaultDisplay::releaseAllResourcesIfUnused()
{
    if (defaultDisplayRefCount)
        return;
    if (defaultDisplay == nullptr)
        return;
    bool result = EGL_Terminate(defaultDisplay);
    ASSERT_UNUSED(result, result);
    result = EGL_ReleaseThread();
    ASSERT_UNUSED(result, result);
    defaultDisplay = nullptr;
}

}

#endif
