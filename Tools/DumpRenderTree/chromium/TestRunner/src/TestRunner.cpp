/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Pawel Hajdan (phajdan.jr@chromium.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TestRunner.h"

#include "WebTestDelegate.h"

namespace WebTestRunner {

TestRunner::TestRunner()
    : m_delegate(0)
{
    // The following are stubs.
    bindMethod("dumpDatabaseCallbacks", &TestRunner::notImplemented);
#if ENABLE(NOTIFICATIONS)
    bindMethod("denyWebNotificationPermission", &TestRunner::notImplemented);
    bindMethod("removeAllWebNotificationPermissions", &TestRunner::notImplemented);
    bindMethod("simulateWebNotificationClick", &TestRunner::notImplemented);
#endif
    bindMethod("setIconDatabaseEnabled", &TestRunner::notImplemented);
    bindMethod("setScrollbarPolicy", &TestRunner::notImplemented);
    bindMethod("clearAllApplicationCaches", &TestRunner::notImplemented);
    bindMethod("clearApplicationCacheForOrigin", &TestRunner::notImplemented);
    bindMethod("clearBackForwardList", &TestRunner::notImplemented);
    bindMethod("keepWebHistory", &TestRunner::notImplemented);
    bindMethod("setApplicationCacheOriginQuota", &TestRunner::notImplemented);
    bindMethod("setCallCloseOnWebViews", &TestRunner::notImplemented);
    bindMethod("setMainFrameIsFirstResponder", &TestRunner::notImplemented);
    bindMethod("setPrivateBrowsingEnabled", &TestRunner::notImplemented);
    bindMethod("setUseDashboardCompatibilityMode", &TestRunner::notImplemented);
    bindMethod("deleteAllLocalStorage", &TestRunner::notImplemented);
    bindMethod("localStorageDiskUsageForOrigin", &TestRunner::notImplemented);
    bindMethod("originsWithLocalStorage", &TestRunner::notImplemented);
    bindMethod("deleteLocalStorageForOrigin", &TestRunner::notImplemented);
    bindMethod("observeStorageTrackerNotifications", &TestRunner::notImplemented);
    bindMethod("syncLocalStorage", &TestRunner::notImplemented);
    bindMethod("addDisallowedURL", &TestRunner::notImplemented);
    bindMethod("applicationCacheDiskUsageForOrigin", &TestRunner::notImplemented);

    // The fallback method is called when an unknown method is invoked.
    bindFallbackMethod(&TestRunner::fallbackMethod);
}

void TestRunner::fallbackMethod(const CppArgumentList&, CppVariant* result)
{
    m_delegate->printMessage("CONSOLE MESSAGE: JavaScript ERROR: unknown method called on TestRunner\n");
    result->setNull();
}

void TestRunner::notImplemented(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

}
