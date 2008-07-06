/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#import "WebNSUserDefaultsExtras.h"

#import "WebNSObjectExtras.h"
#import <WebKitSystemInterface.h>
#import <wtf/Assertions.h>

@implementation NSString (WebNSUserDefaultsPrivate)

- (NSString *)_webkit_HTTPStyleLanguageCode
{
    // Look up the language code using CFBundle.
    NSString *languageCode = self;
    NSString *preferredLanguageCode = WebCFAutorelease(WKCopyCFLocalizationPreferredName((CFStringRef)self));

    if (preferredLanguageCode)
        languageCode = preferredLanguageCode;
    
    // Make the string lowercase.
    NSString *lowercaseLanguageCode = [languageCode lowercaseString];
    
    // Turn a '_' into a '-' if it appears after a 2-letter language code.
    if ([lowercaseLanguageCode length] < 3 || [lowercaseLanguageCode characterAtIndex:2] != '_') {
        return lowercaseLanguageCode;
    }
    NSMutableString *result = [lowercaseLanguageCode mutableCopy];
    [result replaceCharactersInRange:NSMakeRange(2, 1) withString:@"-"];
    return [result autorelease];
}

@end

@implementation NSUserDefaults (WebNSUserDefaultsExtras)

static NSString *preferredLanguageCode = nil;
static NSLock *preferredLanguageLock = nil;
static pthread_once_t preferredLanguageLockOnce = PTHREAD_ONCE_INIT;
static pthread_once_t addDefaultsChangeObserverOnce = PTHREAD_ONCE_INIT;

static void makeLock(void)
{
    preferredLanguageLock = [[NSLock alloc] init]; 
}

+ (void)_webkit_ensureAndLockPreferredLanguageLock
{
    pthread_once(&preferredLanguageLockOnce, makeLock);
    [preferredLanguageLock lock];
}

+ (void)_webkit_defaultsDidChange
{
    [self _webkit_ensureAndLockPreferredLanguageLock];

    [preferredLanguageCode release];
    preferredLanguageCode = nil;

    [preferredLanguageLock unlock];
}

static void addDefaultsChangeObserver(void)
{
    [[NSNotificationCenter defaultCenter] addObserver:[NSUserDefaults class]
                                             selector:@selector(_webkit_defaultsDidChange)
                                                 name:NSUserDefaultsDidChangeNotification
                                               object:[NSUserDefaults standardUserDefaults]];
}

+ (void)_webkit_addDefaultsChangeObserver
{
    pthread_once(&addDefaultsChangeObserverOnce, addDefaultsChangeObserver);
}

+ (NSString *)_webkit_preferredLanguageCode
{
    // Get this outside the lock since it might block on the defaults lock, while we are inside registerDefaults:.
    NSUserDefaults *standardDefaults = [self standardUserDefaults];

    BOOL addObserver = NO;

    [self _webkit_ensureAndLockPreferredLanguageLock];

    if (!preferredLanguageCode) {
        NSArray *languages = [standardDefaults stringArrayForKey:@"AppleLanguages"];
        if ([languages count] == 0) {
            preferredLanguageCode = [@"en" retain];
        } else {
            preferredLanguageCode = [[[languages objectAtIndex:0] _webkit_HTTPStyleLanguageCode] copy];
        }
        addObserver = YES;
    }

    NSString *code = [[preferredLanguageCode retain] autorelease];
    
    [preferredLanguageLock unlock];

    if (addObserver) {
        [self _webkit_addDefaultsChangeObserver];
    }

    return code;
}

@end
