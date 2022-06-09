/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018, 2019 Igalia S.L.
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(TEXTURE_MAPPER) && USE(ANGLE) && !USE(NICOSIA)

#include <wtf/Noncopyable.h>

typedef void *EGLConfig;
typedef void *EGLContext;
typedef void *EGLDisplay;
typedef void *EGLSurface;

namespace WebCore {

class ANGLEContext {
    WTF_MAKE_NONCOPYABLE(ANGLEContext);
public:
    static const char* errorString(int statusCode);
    static const char* lastErrorString();

    static std::unique_ptr<ANGLEContext> createContext(EGLContext sharingContext, bool isForWebGL2);
    ~ANGLEContext();

    bool makeContextCurrent();
    EGLContext platformContext() const;
    EGLDisplay platformDisplay() const;
    EGLConfig platformConfig() const;
private:
    ANGLEContext(EGLDisplay, EGLConfig, EGLContext, EGLSurface);

    EGLDisplay m_display { nullptr };
    EGLConfig m_config { nullptr };
    EGLContext m_context { nullptr };
    EGLSurface m_surface { nullptr };
};

} // namespace WebCore

#endif
