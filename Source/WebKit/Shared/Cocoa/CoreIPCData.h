/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "DataReference.h"

#include <CoreFoundation/CoreFoundation.h>
#include <wtf/RetainPtr.h>
#include <wtf/cocoa/TypeCastsCocoa.h>

namespace WebKit {

class CoreIPCData {
public:

#ifdef __OBJC__
    CoreIPCData(NSData *nsData)
        : CoreIPCData(bridge_cast(nsData))
    {
    }
#endif

    CoreIPCData(CFDataRef cfData)
        : m_cfData(cfData)
        , m_reference(CFDataGetBytePtr(cfData), CFDataGetLength(cfData))
    {
    }

    CoreIPCData(const IPC::DataReference& data)
        : m_reference(data)
    {
    }

    RetainPtr<CFDataRef> createData() const
    {
        return adoptCF(CFDataCreate(0, m_reference.data(), m_reference.size()));
    }

    IPC::DataReference get() const
    {
        return m_reference;
    }

    RetainPtr<id> toID() const
    {
        return bridge_cast(createData().get());
    }

private:
    RetainPtr<CFDataRef> m_cfData;
    IPC::DataReference m_reference;
};

}

#endif // PLATFORM(COCOA)
