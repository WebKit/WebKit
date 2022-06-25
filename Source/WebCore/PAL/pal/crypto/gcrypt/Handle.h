/*
 * Copyright (C) 2017 Metrological Group B.V.
 * Copyright (C) 2017 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <gcrypt.h>

namespace PAL {
namespace GCrypt {

template<typename T>
struct HandleDeleter {
public:
    void operator()(T handle) = delete;
};

template<typename T>
class Handle {
public:
    Handle() = default;

    explicit Handle(T handle)
        : m_handle(handle)
    { }

    ~Handle()
    {
        clear();
    }

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    Handle(Handle&&) = delete;
    Handle& operator=(Handle&&) = delete;

    void clear()
    {
        if (m_handle)
            HandleDeleter<T>()(m_handle);
        m_handle = nullptr;
    }

    T release()
    {
        T handle = m_handle;
        m_handle = nullptr;
        return handle;
    }

    T* operator&() { return &m_handle; }

    T handle() const { return m_handle; }
    operator T() const { return m_handle; }

    bool operator!() const { return !m_handle; }

private:
    T m_handle { nullptr };
};

template<>
struct HandleDeleter<gcry_cipher_hd_t> {
    void operator()(gcry_cipher_hd_t handle)
    {
        gcry_cipher_close(handle);
    }
};

template<>
struct HandleDeleter<gcry_ctx_t> {
    void operator()(gcry_ctx_t handle)
    {
        gcry_ctx_release(handle);
    }
};

template<>
struct HandleDeleter<gcry_mac_hd_t> {
    void operator()(gcry_mac_hd_t handle)
    {
        gcry_mac_close(handle);
    }
};

template<>
struct HandleDeleter<gcry_mpi_t> {
    void operator()(gcry_mpi_t handle)
    {
        gcry_mpi_release(handle);
    }
};

template<>
struct HandleDeleter<gcry_mpi_point_t> {
    void operator()(gcry_mpi_point_t handle)
    {
        gcry_mpi_point_release(handle);
    }
};

template<>
struct HandleDeleter<gcry_sexp_t> {
    void operator()(gcry_sexp_t handle)
    {
        gcry_sexp_release(handle);
    }
};

} // namespace GCrypt
} // namespace PAL
