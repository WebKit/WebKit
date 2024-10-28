/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if HAVE(SEC_ACCESS_CONTROL)

#import <wtf/RetainPtr.h>
#import <wtf/cf/VectorCF.h>
#import <wtf/spi/cocoa/SecuritySPI.h>

namespace WebKit {

class CoreIPCSecAccessControl {
public:
    CoreIPCSecAccessControl(SecAccessControlRef accessControl)
        : m_accessControlData(dataFromAccessControl(accessControl))
    {
    }

    CoreIPCSecAccessControl(RetainPtr<CFDataRef> data)
        : m_accessControlData(data)
    {
    }

    CoreIPCSecAccessControl(std::span<const uint8_t> data)
        : m_accessControlData(adoptCF(CFDataCreate(kCFAllocatorDefault, data.data(), data.size())))
    {
    }

    RetainPtr<SecAccessControlRef> createSecAccessControl() const
    {
        auto accessControl = adoptCF(SecAccessControlCreateFromData(kCFAllocatorDefault, m_accessControlData.get(), NULL));
        ASSERT(accessControl);
        return accessControl;
    }

    std::span<const uint8_t> dataReference() const
    {
        if (!m_accessControlData)
            return { };
        return span(m_accessControlData.get());
    }

private:
    RetainPtr<CFDataRef> dataFromAccessControl(SecAccessControlRef accessControl) const
    {
        ASSERT(accessControl);
        auto data = adoptCF(SecAccessControlCopyData(accessControl));
        ASSERT(data);
        return data;
    }

    RetainPtr<CFDataRef> m_accessControlData;
};

} // namespace WebKit

#endif // HAVE(SEC_ACCESS_CONTROL)
