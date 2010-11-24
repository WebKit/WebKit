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

#import "WebInspectorProxy.h"

#if ENABLE(INSPECTOR)

#import "WKAPICast.h"
#import "WKView.h"
#import "WebPageProxy.h"
#import <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

WebPageProxy* WebInspectorProxy::platformCreateInspectorPage()
{
    ASSERT(m_page);
    ASSERT(!m_inspectorView);

    m_inspectorView.adoptNS([[WKView alloc] initWithFrame:NSZeroRect pageNamespaceRef:toAPI(m_page->pageNamespace())]);
    ASSERT(m_inspectorView);

    return toImpl([m_inspectorView.get() pageRef]);
}

String WebInspectorProxy::inspectorPageURL() const
{
    NSString *path = [[NSBundle bundleWithIdentifier:@"com.apple.WebCore"] pathForResource:@"inspector" ofType:@"html" inDirectory:@"inspector"];
    ASSERT(path);

    return [[NSURL fileURLWithPath:path] absoluteString];
}

} // namespace WebKit

#endif // ENABLE(INSPECTOR)
