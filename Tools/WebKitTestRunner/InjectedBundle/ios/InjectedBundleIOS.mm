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

#import "InjectedBundle.h"

#import <Foundation/Foundation.h>

namespace WTR {

void InjectedBundle::platformInitialize(WKTypeRef)
{
    // Work around missing /etc/catalog <rdar://problem/4292995>.
    setenv("XML_CATALOG_FILES", "", 0);

    // Language was set up earlier in main(). Don't clobber it.
    NSArray *languages = [[[NSUserDefaults standardUserDefaults] volatileDomainForName:NSArgumentDomain] valueForKey:@"AppleLanguages"];

    RetainPtr<NSMutableDictionary> dict = adoptNS([[NSMutableDictionary alloc] init]);
    if (languages)
        [dict setObject:languages forKey:@"AppleLanguages"];

    [[NSUserDefaults standardUserDefaults] setVolatileDomain:dict.get() forName:NSArgumentDomain];
}

} // namespace WTR
