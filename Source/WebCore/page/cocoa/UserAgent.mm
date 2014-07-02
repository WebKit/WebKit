/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "UserAgent.h"

#import "SystemVersion.h"

namespace WebCore {

String systemMarketingVersionForUserAgentString()
{
    // Use underscores instead of dots because when we first added the Mac OS X version to the user agent string
    // we were concerned about old DHTML libraries interpreting "4." as Netscape 4. That's no longer a concern for us
    // but we're sticking with the underscores for compatibility with the format used by older versions of Safari.
    return [systemMarketingVersion() stringByReplacingOccurrencesOfString:@"." withString:@"_"];
}

static NSString *userVisibleWebKitBundleVersionFromFullVersion(NSString *fullWebKitVersion)
{
    // If the version is longer than 3 digits then the leading digits represent the version of the OS. Our user agent
    // string should not include the leading digits, so strip them off and report the rest as the version. <rdar://problem/4997547>
    NSRange nonDigitRange = [fullWebKitVersion rangeOfCharacterFromSet:[[NSCharacterSet decimalDigitCharacterSet] invertedSet]];
    if (nonDigitRange.location == NSNotFound && fullWebKitVersion.length > 3)
        return [fullWebKitVersion substringFromIndex:fullWebKitVersion.length - 3];
    if (nonDigitRange.location != NSNotFound && nonDigitRange.location > 3)
        return [fullWebKitVersion substringFromIndex:nonDigitRange.location - 3];
    return fullWebKitVersion;
}

String userAgentBundleVersionFromFullVersionString(const String& fullWebKitVersion)
{
    // We include at most three components of the bundle version in the user agent string.
    NSString *bundleVersion = userVisibleWebKitBundleVersionFromFullVersion(fullWebKitVersion);
    NSScanner *scanner = [NSScanner scannerWithString:bundleVersion];
    NSInteger periodCount = 0;
    while (true) {
        if (![scanner scanUpToString:@"." intoString:nullptr] || scanner.isAtEnd)
            return bundleVersion;

        if (++periodCount == 3)
            return [bundleVersion substringToIndex:scanner.scanLocation];

        ++scanner.scanLocation;
    }

    ASSERT_NOT_REACHED();
}

} // namespace WebCore
