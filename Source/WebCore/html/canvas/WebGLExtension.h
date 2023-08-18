/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEBGL)

#include <atomic>

namespace WebCore {

class WebCoreOpaqueRoot;

// Mixin class for WebGL extension implementations.
// All functions should start with preamble:
// if (isContextLost())
//     return;
// auto& context = this->context();
// context.drawSomething(...);
template<typename T>
class WebGLExtension {
public:
    void loseParentContext() { m_context = nullptr; }
    T& context() { ASSERT(!isContextLost()); return *m_context.load(std::memory_order::relaxed); }

    // Only to be used by friend WebCoreOpaqueRoot root(const WebGLExtension<T>*) that cannot be a friend
    // due to C++ warning on some compilers.
    T* opaqueRoot() const { return m_context.load(); }

protected:
    WebGLExtension(T& context)
        : m_context(&context)
    {
    }
    bool isContextLost() const { return !m_context.load(std::memory_order::relaxed); }

private:
    std::atomic<T*> m_context;
};

} // namespace WebCore

#endif
