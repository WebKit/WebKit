/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "config.h"

#if WK_HAVE_C_SPI

#include "PlatformUtilities.h"
#include "Test.h"
#include <WebKit/WKSecurityOriginRef.h>

namespace TestWebKitAPI {

TEST(WebKit, WKSecurityOriginCreateInvalidPortZero)
{
    auto protocol = adoptWK(WKStringCreateWithUTF8CString("https"));
    auto host = adoptWK(WKStringCreateWithUTF8CString("www.apple.com"));
    auto origin = adoptWK(WKSecurityOriginCreate(protocol.get(), host.get(), 0));

    auto copiedProtocol = adoptWK(WKSecurityOriginCopyProtocol(origin.get()));
    EXPECT_WK_STREQ("https", copiedProtocol.get());
    auto copiedHost = adoptWK(WKSecurityOriginCopyHost(origin.get()));
    EXPECT_WK_STREQ("www.apple.com", copiedHost.get());
    auto string = adoptWK(WKSecurityOriginCopyToString(origin.get()));
    EXPECT_WK_STREQ("https://www.apple.com", string.get());
    EXPECT_EQ(0U, WKSecurityOriginGetPort(origin.get()));
}

TEST(WebKit, WKSecurityOriginCreateInvalidPortTooLarge)
{
    auto protocol = adoptWK(WKStringCreateWithUTF8CString("https"));
    auto host = adoptWK(WKStringCreateWithUTF8CString("www.apple.com"));
    auto origin = adoptWK(WKSecurityOriginCreate(protocol.get(), host.get(), 99999));

    auto copiedProtocol = adoptWK(WKSecurityOriginCopyProtocol(origin.get()));
    EXPECT_WK_STREQ("https", copiedProtocol.get());
    auto copiedHost = adoptWK(WKSecurityOriginCopyHost(origin.get()));
    EXPECT_WK_STREQ("www.apple.com", copiedHost.get());
    auto string = adoptWK(WKSecurityOriginCopyToString(origin.get()));
    EXPECT_WK_STREQ("https://www.apple.com", string.get());
    EXPECT_EQ(0U, WKSecurityOriginGetPort(origin.get()));
}

} // namespace TestWebKitAPI

#endif // WK_HAVE_C_SPI
