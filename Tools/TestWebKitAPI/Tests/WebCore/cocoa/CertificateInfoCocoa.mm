/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "config.h"

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/HTTPServer/TestCertificates.h"
#import <WebCore/CertificateInfo.h>
#import <WebCore/CertificateSummary.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

static WebCore::CertificateInfo certificateInfoForTestCertificate()
{
    RetainPtr certificate = testCertificate();
    const void* certificates[] = { certificate.get() };
    RetainPtr chain = adoptCF(CFArrayCreate(kCFAllocatorDefault, certificates, 1, &kCFTypeArrayCallBacks));
    return WebCore::CertificateInfo { WebCore::CertificateInfo::secTrustFromCertificateChain(chain.get()) };
}

// The test certificate (see TestCertificates.swift) is valid from 2019-03-23 05:50:14 UTC
// until 2020-03-22 05:50:14 UTC, and its common name is "Me".
TEST(CertificateInfo, SummaryHasSubjectAndValidityDates)
{
    auto certificateInfo = certificateInfoForTestCertificate();
    ASSERT_FALSE(certificateInfo.isEmpty());

    auto summary = certificateInfo.summary();
    ASSERT_TRUE(summary.has_value());

    EXPECT_WK_STREQ("Me", summary->subject);
    EXPECT_EQ(1553320214.0, summary->validFrom.seconds());
    EXPECT_EQ(1584856214.0, summary->validUntil.seconds());
}

} // namespace TestWebKitAPI
