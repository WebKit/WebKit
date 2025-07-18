/*
 * Copyright (C) 2023 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wayland-client.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/unix/UnixFileDescriptor.h>

namespace WPE {

class WaylandSHMPool {
    WTF_MAKE_TZONE_ALLOCATED(WaylandSHMPool);
public:
    static std::unique_ptr<WaylandSHMPool> create(struct wl_shm*, size_t);

    WaylandSHMPool(void*, size_t, WTF::UnixFileDescriptor&&, struct wl_shm*);
    ~WaylandSHMPool();

    int allocate(size_t);
    struct wl_buffer* createBuffer(uint32_t offset, uint32_t width, uint32_t height, uint32_t stride);
    void write(std::span<const uint8_t> data, int offset = 0);
    void write(std::span<const uint32_t> data, int offset = 0);

private:
    bool resize(size_t);
    std::span<uint8_t> mutableSpan() LIFETIME_BOUND { return unsafeMakeSpan<uint8_t>(static_cast<uint8_t*>(m_data), m_size); }

    void* m_data { nullptr };
    size_t m_size { 0 };
    WTF::UnixFileDescriptor m_fd;
    struct wl_shm_pool* m_pool { nullptr };
    size_t m_used { 0 };
};

} // namespace WPE
