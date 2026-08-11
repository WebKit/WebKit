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
#import "Helpers/DeprecatedGlobalValues.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import <WebCore/SecurityPolicy.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebViewPrivate.h>
#import <wtf/FileHandle.h>
#import <wtf/FileSystem.h>
#import <wtf/RetainPtr.h>
#import <wtf/URL.h>

#if PLATFORM(IOS_FAMILY)
#import <WebKit/WebUIKitSupport.h>
#endif

@interface SubstituteDataLocalResourceAccessFrameLoadDelegate : NSObject <WebFrameLoadDelegate>
@end

@implementation SubstituteDataLocalResourceAccessFrameLoadDelegate

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}

@end

namespace TestWebKitAPI {

// A document loaded via -loadHTMLString:baseURL: is loaded with SubstituteData. Its origin is
// derived from the base URL (here http:, which is not a local scheme), so it does not qualify for
// local resource access on its own. Loading a classic <script src> from a file: URL must succeed
// via the local-content grant applied in DocumentLoader::commitData(), which only applies to the
// document that was constructed with the substitute data. A classic script load uses NoCors mode,
// which is gated by SecurityOrigin::canDisplay() -- unlike fetch()'s default SameOrigin mode,
// which is always same-origin-restricted regardless of the grant.
//
// The script communicates its result via window.testResult, read back with
// -stringByEvaluatingJavaScriptFromString: after the load completes, rather than a native
// callback bridge installed through -webView:didCreateJavaScriptContext:forFrame:.
TEST(WebKitLegacy, SubstituteDataDocumentCanLoadLocalResource)
{
    didFinishLoad = false;

    auto [tempFilePath, tempFileHandle] = FileSystem::openTemporaryFile("SubstituteDataLocalResourceAccess"_s, ".js"_s);
    ASCIILiteral scriptContents = "window.testResult = 'local file contents'"_s;
    tempFileHandle.write(scriptContents.span8());
    tempFileHandle = { };

    URL tempFileURL = URL::fileURLWithFileSystemPath(tempFilePath);
    RetainPtr nsTempFileURL = tempFileURL.createNSURL();

    RetainPtr preferences = adoptNS([[WebPreferences alloc] init]);
    preferences.get().webSecurityEnabled = YES;
    preferences.get().allowUniversalAccessFromFileURLs = NO;

#if PLATFORM(IOS_FAMILY)
    // WebKitInitialize() is normally called by UIKit before any WebView is created; call it
    // directly here (without linking UIKit) since this test creates a WebView on its own.
    WebKitInitialize();
#endif

    RetainPtr webView = adoptNS([[WebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) frameName:nil groupName:nil]);
    webView.get().preferences = preferences.get();

    // -initWithFrame:frameName:groupName: only sets AllowLocalLoadsForLocalAndSubstituteData when the
    // app is linked against an SDK older than WEBKIT_FIRST_VERSION_WITH_MORE_STRICT_LOCAL_RESOURCE_SECURITY_RESTRICTION
    // (2008-era), which is never true for a modern test binary. Set the policy directly so the
    // local-content grant this test exercises is actually reachable.
    WebCore::SecurityPolicy::setLocalLoadPolicy(WebCore::SecurityPolicy::AllowLocalLoadsForLocalAndSubstituteData);

    RetainPtr delegate = adoptNS([[SubstituteDataLocalResourceAccessFrameLoadDelegate alloc] init]);
    webView.get().frameLoadDelegate = delegate.get();

    RetainPtr html = adoptNS([[NSString alloc] initWithFormat:@"<script>window.onerror = () => window.testResult = 'script failed'</script><script src=\"%s\" onerror=\"window.testResult = 'script failed'\"></script>", nsTempFileURL.get().absoluteString.UTF8String]);
    [[webView mainFrame] loadHTMLString:html.get() baseURL:[NSURL URLWithString:@"http://example.com/"]];
    Util::run(&didFinishLoad);

    WebCore::SecurityPolicy::setLocalLoadPolicy(WebCore::SecurityPolicy::AllowLocalLoadsForLocalOnly);

    NSString *scriptResult = [webView.get() stringByEvaluatingJavaScriptFromString:@"window.testResult"];
    EXPECT_WK_STREQ("local file contents", scriptResult);

    FileSystem::deleteFile(tempFilePath);
}

} // namespace TestWebKitAPI
