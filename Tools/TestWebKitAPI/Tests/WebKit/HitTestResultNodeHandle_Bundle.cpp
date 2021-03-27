/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#if ENABLE(CONTEXT_MENUS)

#include "InjectedBundleTest.h"
#include "InjectedBundleController.h"
#include "PlatformUtilities.h"
#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundleHitTestResult.h>
#include <WebKit/WKRetainPtr.h>

namespace TestWebKitAPI {

class HitTestResultNodeHandleTest : public InjectedBundleTest {
public:
    HitTestResultNodeHandleTest(const std::string& identifier)
        : InjectedBundleTest(identifier)
    {
    }

    static void getContextMenuFromDefaultMenu(WKBundlePageRef page, WKBundleHitTestResultRef hitTestResult, WKArrayRef defaultMenu, WKArrayRef* newMenu, WKTypeRef* userData, const void* clientInfo)
    {
        WKRetainPtr<WKBundleNodeHandleRef> nodeHandle = adoptWK(WKBundleHitTestResultCopyNodeHandle(hitTestResult));
        if (!nodeHandle)
            return;
        
        WKBundlePostMessage(InjectedBundleController::singleton().bundle(), Util::toWK("HitTestResultNodeHandleTestDoneMessageName").get(), Util::toWK("HitTestResultNodeHandleTestDoneMessageBody").get());
    }

    virtual void didCreatePage(WKBundleRef bundle, WKBundlePageRef page)
    {
        WKBundlePageContextMenuClientV0 contextMenuClient;
        memset(&contextMenuClient, 0, sizeof(contextMenuClient));

        contextMenuClient.base.version = 0;
        contextMenuClient.getContextMenuFromDefaultMenu = getContextMenuFromDefaultMenu;
    
        WKBundlePageSetContextMenuClient(page, &contextMenuClient.base);
    }
};

static InjectedBundleTest::Register<HitTestResultNodeHandleTest> registrar("HitTestResultNodeHandleTest");

} // namespace TestWebKitAPI

#endif

#endif
