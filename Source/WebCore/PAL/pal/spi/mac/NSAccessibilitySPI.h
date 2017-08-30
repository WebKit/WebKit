/*
 * Copyright (C) 2015 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if USE(APPKIT)

#if USE(APPLE_INTERNAL_SDK)

#import <AppKit/NSAccessibilityRemoteUIElement.h>

#else

@interface NSAccessibilityRemoteUIElement : NSObject

+ (void)setRemoteUIApp:(BOOL)flag;
+ (NSData *)remoteTokenForLocalUIElement:(id)localUIElement;
+ (void)registerRemoteUIProcessIdentifier:(pid_t)pid;
+ (void)unregisterRemoteUIProcessIdentifier:(pid_t)pid;

- (id)initWithRemoteToken:(NSData *)remoteToken;
- (pid_t)processIdentifier;
- (void)accessibilitySetPresenterProcessIdentifier:(pid_t)presenterPID;

@property (retain) id windowUIElement;
@property (retain) id topLevelUIElement;

@end

#endif // USE(APPLE_INTERNAL_SDK)

WTF_EXTERN_C_BEGIN

void NSAccessibilityHandleFocusChanged();
void NSAccessibilityUnregisterUniqueIdForUIElement(id element);

WTF_EXTERN_C_END

#endif // USE(APPKIT)
