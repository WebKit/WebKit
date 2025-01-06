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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionAPIWebPageNamespace.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "WebExtensionAPINamespace.h"
#import "WebExtensionAPIRuntime.h"
#import "WebExtensionAPITest.h"
#import "WebExtensionContextProxy.h"
#import "WebExtensionControllerProxy.h"
#import "WebPage.h"

namespace WebKit {

bool WebExtensionAPIWebPageNamespace::isPropertyAllowed(const ASCIILiteral& name, WebPage* page)
{
    if (name == "test"_s) {
        if (!page)
            return false;
        if (RefPtr extensionController = page->webExtensionControllerProxy())
            return extensionController->inTestingMode();
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

WebExtensionAPIWebPageRuntime& WebExtensionAPIWebPageNamespace::runtime() const
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime

    if (!m_runtime) {
        m_runtime = WebExtensionAPIWebPageRuntime::create(contentWorldType());
        m_runtime->setPropertyPath("runtime"_s, this);
    }

    return *m_runtime;
}

WebExtensionAPITest& WebExtensionAPIWebPageNamespace::test()
{
    // Documentation: None (Testing Only)

    if (!m_test)
        m_test = WebExtensionAPITest::create(*this);

    return *m_test;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
