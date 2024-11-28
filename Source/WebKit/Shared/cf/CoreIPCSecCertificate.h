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

#if USE(CF)

#import <Security/SecCertificate.h>
#import <wtf/RetainPtr.h>
#import <wtf/cf/VectorCF.h>

namespace WebKit {

class CoreIPCSecCertificate {
public:
    CoreIPCSecCertificate(SecCertificateRef certificate)
        : m_certificateData(dataFromCertificate(certificate))
    {
    }

    CoreIPCSecCertificate(RetainPtr<CFDataRef> data)
        : m_certificateData(data)
    {
    }

    CoreIPCSecCertificate(std::span<const uint8_t> data)
        : m_certificateData(adoptCF(CFDataCreate(kCFAllocatorDefault, data.data(), data.size())))
    {
    }

    RetainPtr<SecCertificateRef> createSecCertificate() const
    {
        auto certificate = adoptCF(SecCertificateCreateWithData(kCFAllocatorDefault, m_certificateData.get()));
        ASSERT(certificate);
        return certificate;
    }

    std::span<const uint8_t> dataReference() const
    {
        RELEASE_ASSERT(m_certificateData);
        return span(m_certificateData.get());
    }

private:
    RetainPtr<CFDataRef> dataFromCertificate(SecCertificateRef certificate) const
    {
        ASSERT(certificate);
        auto data = adoptCF(SecCertificateCopyData(certificate));
        ASSERT(data);
        return data;
    }

    RetainPtr<CFDataRef> m_certificateData;
};

} // namespace WebKit

#endif // USE(CF)
