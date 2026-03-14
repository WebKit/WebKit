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

// Test for rdar://134640814 / https://bugs.webkit.org/show_bug.cgi?id=309182
// Verifies that stopping a module service worker while it is still loading
// does not crash the WebContent process. The bug was that WorkerMainRunLoop::runInMode()
// unconditionally returned true, so loadModuleSynchronously() continued to call
// performMicrotaskCheckpoint() after the worker's m_script had been cleared.

#import "config.h"

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/RetainPtr.h>

// The module service worker script imports a dependency that will never finish loading.
static constexpr auto swModuleScript = R"SWRESOURCE(
import './slow-dep.mjs';
)SWRESOURCE"_s;

TEST(ServiceWorkers, ModuleUnregisterDuringLoadNoMainThreadCrash)
{
    TestWebKitAPI::HTTPServer server({
        { "/sw-module.mjs"_s, { { { "Content-Type"_s, "application/javascript"_s } }, swModuleScript } },
        // The dependency never responds, keeping the module load pending.
        { "/slow-dep.mjs"_s, { TestWebKitAPI::HTTPResponse::Behavior::NeverSendResponse } },
    });

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    auto storeConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);

    // Start with a clean slate data store.
    __block bool cleanDone = false;
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^{
        cleanDone = true;
    }];
    TestWebKitAPI::Util::run(&cleanDone);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore.get()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    // Use _loadServiceWorker:usingModules: to exercise the production code path
    // (used by Web Extensions / Safari) where the service worker runs on the main
    // thread via WorkerMainRunLoop, triggered by serviceWorkerPageIdentifier.
    //
    // We cannot wait for the completion handler because for main-thread module workers,
    // evaluateScriptIfNecessary() calls loadModuleSynchronously() synchronously, which
    // blocks until the module graph is loaded. Since slow-dep.mjs never responds,
    // the evaluate callback never fires and the registration never completes.
    [webView _loadServiceWorker:server.request("/sw-module.mjs"_s).URL usingModules:YES completionHandler:^(BOOL) { }];

    // Wait until the server receives the request for /slow-dep.mjs. At this point the
    // worker is confirmed to be inside loadModuleSynchronously(), spinning
    // RunLoop::main().cycle() waiting for the module dependency to load.
    // Request 1 = /sw-module.mjs, request 2 = /slow-dep.mjs.
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return server.totalRequests() >= 2;
    }));

    // Closing the view removes the service worker page client, which triggers
    // the registration to be cleared and the worker to be stopped while the
    // module is still loading. The stop task is dispatched to the main run loop
    // and picked up by loadModuleSynchronously()'s RunLoop::main().cycle() call.
    //
    // If the bug were present (WorkerMainRunLoop::runInMode() unconditionally
    // returning true), the WebContent process would crash with SIGSEGV at
    // address 0x8 when performMicrotaskCheckpoint() called vm() on a null m_script.
    [webView _close];
    webView = nil;

    // Spin the run loop to allow the close to propagate through IPC and any
    // crash to manifest.
    TestWebKitAPI::Util::spinRunLoop(100);
}
