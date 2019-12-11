/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import <WebKit/WKFoundation.h>

#if WK_HAVE_C_SPI

#import "PlatformUtilities.h"
#import "PlatformWebView.h"
#import "Test.h"
#import <WebKit/WKPageGroup.h>
#import <WebKit/WKUserContentControllerRef.h>
#import <WebKit/WKPageConfigurationRef.h>

namespace TestWebKitAPI {

TEST(PageGroup, DefaultUserContentController)
{
    auto pageConfiguration = adoptWK(WKPageConfigurationCreate());
    auto context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    WKPageConfigurationSetContext(pageConfiguration.get(), context.get());
    auto pageGroup = adoptWK(WKPageGroupCreateWithIdentifier(Util::toWK("TestPageGroup").get()));
    WKPageConfigurationSetPageGroup(pageConfiguration.get(), pageGroup.get());

    auto pageGroupUserContentController = retainWK(WKPageGroupGetUserContentController(pageGroup.get()));

    EXPECT_NULL(WKPageConfigurationGetUserContentController(pageConfiguration.get()));

    PlatformWebView webView(pageConfiguration.get());
    auto copiedPageConfiguration = adoptWK(WKPageCopyPageConfiguration(webView.page()));

    ASSERT_EQ(pageGroupUserContentController.get(), WKPageConfigurationGetUserContentController(copiedPageConfiguration.get()));
}

TEST(PageGroup, CustomUserContentController)
{
    auto pageConfiguration = adoptWK(WKPageConfigurationCreate());
    auto context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    WKPageConfigurationSetContext(pageConfiguration.get(), context.get());
    auto pageGroup = adoptWK(WKPageGroupCreateWithIdentifier(Util::toWK("TestPageGroup").get()));
    WKPageConfigurationSetPageGroup(pageConfiguration.get(), pageGroup.get());
    auto userContentController = adoptWK(WKUserContentControllerCreate());
    WKPageConfigurationSetUserContentController(pageConfiguration.get(), userContentController.get());

    auto pageGroupUserContentController = retainWK(WKPageGroupGetUserContentController(pageGroup.get()));

    EXPECT_EQ(userContentController.get(), WKPageConfigurationGetUserContentController(pageConfiguration.get()));

    PlatformWebView webView(pageConfiguration.get());
    auto copiedPageConfiguration = adoptWK(WKPageCopyPageConfiguration(webView.page()));

    EXPECT_EQ(userContentController.get(), WKPageConfigurationGetUserContentController(copiedPageConfiguration.get()));
}

} // namespace TestWebKitAPI

#endif
