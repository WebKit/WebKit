/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#include <mach/mach_port.h>

namespace WTF {

class MachSendRight {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WTF_EXPORT_PRIVATE static MachSendRight adopt(mach_port_t);
    WTF_EXPORT_PRIVATE static MachSendRight create(mach_port_t);

    MachSendRight() = default;
    WTF_EXPORT_PRIVATE MachSendRight(const MachSendRight&);
    WTF_EXPORT_PRIVATE MachSendRight(MachSendRight&&);
    WTF_EXPORT_PRIVATE ~MachSendRight();

    WTF_EXPORT_PRIVATE MachSendRight& operator=(const MachSendRight&);
    WTF_EXPORT_PRIVATE MachSendRight& operator=(MachSendRight&&);

    explicit operator bool() const { return m_port != MACH_PORT_NULL; }

    mach_port_t sendRight() const { return m_port; }

    WTF_EXPORT_PRIVATE MachSendRight copySendRight() const;
    WTF_EXPORT_PRIVATE mach_port_t leakSendRight() WARN_UNUSED_RETURN;

private:
    explicit MachSendRight(mach_port_t);

    mach_port_t m_port { MACH_PORT_NULL };
};

WTF_EXPORT_PRIVATE void deallocateSendRightSafely(mach_port_t);

}

using WTF::MachSendRight;
using WTF::deallocateSendRightSafely;

#endif
