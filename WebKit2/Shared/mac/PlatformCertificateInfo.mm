/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "PlatformCertificateInfo.h"

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include <WebKitSystemInterface.h>
#include <Security/Security.h>

using namespace WebCore;

namespace WebKit {

PlatformCertificateInfo::PlatformCertificateInfo()
{
}

PlatformCertificateInfo::PlatformCertificateInfo(const ResourceResponse& response)
    : m_peerCertificates(AdoptCF, WKCopyNSURLResponsePeerCertificates(response.nsURLResponse()))
{
}

void PlatformCertificateInfo::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    // Special case no certificates, 
    if (!m_peerCertificates) {
        encoder->encodeUInt64(std::numeric_limits<uint64_t>::max());
        return;
    }

    uint64_t length = CFArrayGetCount(m_peerCertificates.get());
    encoder->encodeUInt64(length);

    for (size_t i = 0; i < length; ++i) {
        RetainPtr<CFDataRef> data(AdoptCF, SecCertificateCopyData((SecCertificateRef)CFArrayGetValueAtIndex(m_peerCertificates.get(), i)));
        encoder->encodeBytes(CFDataGetBytePtr(data.get()), CFDataGetLength(data.get()));
    }
}

bool PlatformCertificateInfo::decode(CoreIPC::ArgumentDecoder* decoder, PlatformCertificateInfo& c)
{
    uint64_t length;
    if (!decoder->decode(length))
        return false;

    if (length == std::numeric_limits<uint64_t>::max()) {
        // This is the no certificates case.
        return true;
    }

    RetainPtr<CFMutableArrayRef> array(AdoptCF, CFArrayCreateMutable(0, length, &kCFTypeArrayCallBacks));

    for (size_t i = 0; i < length; ++i) {
        Vector<uint8_t> bytes;
        if (!decoder->decodeBytes(bytes))
            return false;

        RetainPtr<CFDataRef> data(AdoptCF, CFDataCreateWithBytesNoCopy(0, bytes.data(), bytes.size(), kCFAllocatorNull));
        RetainPtr<SecCertificateRef> certificate(AdoptCF, SecCertificateCreateWithData(0, data.get()));
        CFArrayAppendValue(array.get(), certificate.get());
    }

    c.m_peerCertificates = array;
    return true;
}

#ifndef NDEBUG
void PlatformCertificateInfo::dump() const
{
    unsigned entries = m_peerCertificates ? CFArrayGetCount(m_peerCertificates.get()) : 0;

    NSLog(@"PlatformCertificateInfo\n");
    NSLog(@"  Entries: %d\n", entries);
    for (unsigned i = 0; i < entries; ++i) {
        RetainPtr<CFStringRef> summary(AdoptCF, SecCertificateCopySubjectSummary((SecCertificateRef)CFArrayGetValueAtIndex(m_peerCertificates.get(), i)));
        NSLog(@"  %@", (NSString *)summary.get());
    }
}
#endif

} // namespace WebKit
