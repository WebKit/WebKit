/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#include <mach/message.h>

namespace IPC {

class ImportanceAssertion {
public:
    ImportanceAssertion() = default;

    explicit ImportanceAssertion(mach_msg_header_t* header)
    {
        if (MACH_MSGH_BITS_HAS_VOUCHER(header->msgh_bits)) {
            m_voucher = std::exchange(header->msgh_voucher_port, MACH_VOUCHER_NULL);
            header->msgh_bits &= ~(MACH_MSGH_BITS_VOUCHER_MASK | MACH_MSGH_BITS_RAISEIMP);
        }
    }

    ImportanceAssertion(ImportanceAssertion&& other)
        : m_voucher(std::exchange(other.m_voucher, MACH_VOUCHER_NULL))
    {
    }

    ImportanceAssertion& operator=(ImportanceAssertion&& other)
    {
        if (&other != this)
            std::swap(m_voucher, other.m_voucher);
        return *this;
    }

    ImportanceAssertion(const ImportanceAssertion&) = delete;
    ImportanceAssertion& operator=(const ImportanceAssertion&) = delete;

    ~ImportanceAssertion()
    {
        if (!m_voucher)
            return;

        kern_return_t kr = mach_voucher_deallocate(m_voucher);
        ASSERT_UNUSED(kr, !kr);
        m_voucher = MACH_VOUCHER_NULL;
    }

private:
    mach_voucher_t m_voucher { MACH_VOUCHER_NULL };
};

}

#endif // PLATFORM(MAC)
