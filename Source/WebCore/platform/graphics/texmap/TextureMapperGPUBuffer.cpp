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

#include "config.h"
#include "TextureMapperGPUBuffer.h"

namespace WebCore {

TextureMapperGPUBuffer::TextureMapperGPUBuffer(size_t size, Type type, Usage usage)
    : m_size(size)
    , m_target((type == Type::Vertex) ? GL_ARRAY_BUFFER : GL_ELEMENT_ARRAY_BUFFER)
    , m_usage((usage == Usage::Dynamic) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW)
{
    if (!size)
        return;

    glGenBuffers(1, &m_id);
    if (m_id) {
        glBindBuffer(m_target, m_id);
        glBufferData(m_target, m_size, nullptr, m_usage);
        if (glGetError() != GL_NO_ERROR) {
            glDeleteBuffers(1, &m_id);
            m_id = 0;
        }
    }
}

TextureMapperGPUBuffer::~TextureMapperGPUBuffer()
{
    if (m_id) {
        glDeleteBuffers(1, &m_id);
        m_id = 0;
    }
}

bool TextureMapperGPUBuffer::updateData(const void* data, size_t offset, size_t size)
{
    if (m_id) {
        glBindBuffer(m_target, m_id);
        glBufferData(m_target, m_size, nullptr, m_usage); // Invalidate. No need to preserve previous content
        if (glGetError() != GL_NO_ERROR)
            return false;
        glBufferSubData(m_target, offset, size, data);
        return true;
    }
    return false;
}

} // namespace WebCore
