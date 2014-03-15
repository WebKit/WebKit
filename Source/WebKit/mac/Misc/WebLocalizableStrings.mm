/*
 * Copyright (C) 2005 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKit/WebLocalizableStrings.h>

#import <wtf/Assertions.h>
#import <wtf/MainThread.h>

#if PLATFORM(IOS)
#import "WebLocalizableStringsInternal.h"
#import <dispatch/dispatch.h>
#endif

WebLocalizableStringsBundle WebKitLocalizableStringsBundle = { "com.apple.WebKit", 0 };

NSString *WebLocalizedString(WebLocalizableStringsBundle *stringsBundle, const char *key)
{
    // This function is not thread-safe due at least to its unguarded use of the mainBundle static variable
    // and its use of [NSBundle localizedStringForKey:::], which is not guaranteed to be thread-safe. If
    // we decide we need to use this on background threads, we'll need to add locking here and make sure
    // it doesn't affect performance.
#if !PLATFORM(IOS)
    ASSERT(isMainThread());
#endif

    NSBundle *bundle;
    if (stringsBundle == NULL) {
        static NSBundle *mainBundle;
        if (mainBundle == nil) {
            mainBundle = [NSBundle mainBundle];
            ASSERT(mainBundle);
            CFRetain(mainBundle);
        }
        bundle = mainBundle;
    } else {
        bundle = stringsBundle->bundle;
        if (bundle == nil) {
            bundle = [NSBundle bundleWithIdentifier:[NSString stringWithUTF8String:stringsBundle->identifier]];
            ASSERT(bundle);
            stringsBundle->bundle = bundle;
        }
    }
    NSString *notFound = @"localized string not found";
    CFStringRef keyString = CFStringCreateWithCStringNoCopy(NULL, key, kCFStringEncodingUTF8, kCFAllocatorNull);
    NSString *result = [bundle localizedStringForKey:(NSString *)keyString value:notFound table:nil];
    CFRelease(keyString);
    ASSERT_WITH_MESSAGE(result != notFound, "could not find localizable string %s in bundle", key);
    return result;
}

#if PLATFORM(IOS)
// See <rdar://problem/7902473> Optimize WebLocalizedString for why we do this on a background thread on a timer callback
static void LoadWebLocalizedStringsTimerCallback(CFRunLoopTimerRef timer, void *info)
{
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0), ^{ 
        UI_STRING_KEY_INTERNAL("Typing", "Typing (Undo action name)", "We don't care if we find this string, but searching for it will load the plist and save the results.");
    });
}

void LoadWebLocalizedStrings(void)
{
    CFRunLoopTimerRef timer = CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent(), 0, 0, 0, &LoadWebLocalizedStringsTimerCallback, NULL);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopCommonModes);
    CFRelease(timer);
}
#endif
