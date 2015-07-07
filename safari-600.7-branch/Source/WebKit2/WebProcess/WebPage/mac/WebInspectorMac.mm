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

#import "config.h"
#import "WebInspector.h"

#if ENABLE(INSPECTOR)

#import <WebCore/SoftLinking.h>

SOFT_LINK_STAGED_FRAMEWORK(WebInspectorUI, PrivateFrameworks, A)

namespace WebKit {

bool WebInspector::canSave() const
{
    return true;
}

String WebInspector::localizedStringsURL() const
{
    if (!m_hasLocalizedStringsURL) {
        // Call the soft link framework function to dlopen it, then [NSBundle bundleWithIdentifier:] will work.
        WebInspectorUILibrary();

        NSString *path = [[NSBundle bundleWithIdentifier:@"com.apple.WebInspectorUI"] pathForResource:@"localizedStrings" ofType:@"js"];
        if ([path length])
            m_localizedStringsURL = [[NSURL fileURLWithPath:path] absoluteString];
        else
            m_localizedStringsURL = String();
        m_hasLocalizedStringsURL = true;
    }
    return m_localizedStringsURL;
}

} // namespace WebKit

#endif // ENABLE(INSPECTOR)
