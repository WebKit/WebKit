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

#import "config.h"
#import "InjectedBundle.h"

#import <Foundation/Foundation.h>

@interface NSSound ()
+ (void)_setAlertType:(NSUInteger)alertType;
@end

namespace WTR {

void InjectedBundle::platformInitialize(WKTypeRef initializationUserData)
{
    static const int BlueTintedAppearance = 1;

    // Work around missing /etc/catalog <rdar://problem/4292995>.
    setenv("XML_CATALOG_FILES", "", 0);
    setenv("XTYPE_ALLOW_AUTOACTIVATION", "1", 0);

    // Language was set up earlier in main(). Don't clobber it.
    NSArray *languages = [[[NSUserDefaults standardUserDefaults] volatileDomainForName:NSArgumentDomain] valueForKey:@"AppleLanguages"];
    if (!languages)
        languages = @[ @"en" ];

    NSDictionary *dict = @{
        @"AppleAntiAliasingThreshold": @4,
        @"AppleAquaColorVariant": @(BlueTintedAppearance),
        @"AppleHighlightColor": @"0.709800 0.835300 1.000000",
        @"AppleOtherHighlightColor": @"0.500000 0.500000 0.500000",
        @"NSScrollAnimationEnabled": @NO,
        @"NSOverlayScrollersEnabled": @NO,
        @"AppleShowScrollBars": @"Always",
        @"NSButtonAnimationsEnabled": @NO, // Ideally, we should find a way to test animations, but for now, make sure that the dumped snapshot matches actual state.
        // FIXME (<rdar://problem/13396515>): It is too late to set AppleLanguages here, as loaded frameworks localizations cannot be changed.
        // This breaks some accessibility tests on machines with non-English user language.
        @"AppleLanguages": languages,
        @"NSPreferredSpellServerLanguage": @"en_US",
        @"NSUserDictionaryReplacementItems": @[],
        @"NSTestCorrectionDictionary": @{
            @"notationl": @"notational",
            @"mesage": @"message",
            @"wouldn": @"would",
            @"wellcome": @"welcome",
            @"hellolfworld": @"hello\nworld"
        },
        @"AppleEnableSwipeNavigateWithScrolls": @YES,
        @"com.apple.swipescrolldirection": @1,
        @"com.apple.trackpad.forceClick": @1,
    };

    [[NSUserDefaults standardUserDefaults] setVolatileDomain:dict forName:NSArgumentDomain];

    // Underlying frameworks have already read AppleAntiAliasingThreshold default before we changed it.
    // A distributed notification is delivered to all applications, but it should be harmless, and it's the only way to update all underlying frameworks anyway.
    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:@"AppleAquaAntiAliasingChanged" object:nil userInfo:nil deliverImmediately:YES];

    [NSSound _setAlertType:0];
}

} // namespace WTR
