/*
 * Copyright (C) 2024 Jani Hautakangas <jani@kodegood.com>
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

#include "TextureMapperGLHeaders.h"
#include <wtf/Ref.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class TextureMapperGPUBuffer final : public ThreadSafeRefCounted<TextureMapperGPUBuffer> {
    WTF_MAKE_NONCOPYABLE(TextureMapperGPUBuffer);
public:
    enum class Type : uint8_t {
        Vertex,
        Index
    };

    enum class Usage : uint8_t {
        Dynamic,
        Static
    };

    static Ref<TextureMapperGPUBuffer> create(size_t size, Type type, Usage usage)
    {
        return adoptRef(*new TextureMapperGPUBuffer(size, type, usage));
    }

    ~TextureMapperGPUBuffer();

    GLuint bufferID() const { return m_id; }

    bool updateData(const void*, size_t, size_t);

private:
    TextureMapperGPUBuffer(size_t, Type, Usage);

    size_t m_size;
    GLenum m_target;
    GLenum m_usage;

    GLuint m_id { 0 };
};

} // namespace WebCore
