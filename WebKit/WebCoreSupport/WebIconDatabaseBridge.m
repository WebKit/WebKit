/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#import "WebIconDatabaseBridge.h"

#import "WebIconDatabasePrivate.h"
#import <JavaScriptCore/Assertions.h>

@implementation WebIconDatabaseBridge

// Only sharedInstance is allowed to create the bridge.
// Return nil if someone tries to init.
- (id)init
{
    [self release];
    return nil;
}

- (id)_init
{
    self = [super init];
    return self;
}

// FIXME rdar://4668102 - This is a likely place to add an NSNotification here to notify the app of the updated icon
- (void)_setIconData:(NSData *)data forIconURL:(NSString *)iconURL
{
    [super _setIconData:data forIconURL:iconURL];
}

// FIXME rdar://4668102 - This is a likely place to add an NSNotification here to notify the app of the updated icon
- (void)_setHaveNoIconForIconURL:(NSString *)iconURL
{
    [super _setHaveNoIconForIconURL:iconURL];
}

+ (WebCoreIconDatabaseBridge *)sharedInstance
{
    static WebCoreIconDatabaseBridge* bridge = nil;
    if (!bridge) {
        // Need to CFRetain something that's in a global variable, since we want it to
        // hang around forever, even when running under GC.
        bridge = [[WebIconDatabaseBridge alloc] _init];
        CFRetain(bridge);
        [bridge release];
    }
    return bridge;
}

- (void)dealloc
{
    // The single instance should be kept around forever, so this code should never be reached.
    ASSERT(false);
    [super dealloc];
}

- (void)finalize
{
    // The single instance should be kept around forever, so this code should never be reached.
    ASSERT(false);
    [super finalize];
}

@end
