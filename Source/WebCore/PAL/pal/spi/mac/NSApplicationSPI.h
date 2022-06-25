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

@interface NSApplication ()

- (void)speakString:(NSString *)string;
- (void)_setCurrentEvent:(NSEvent *)event;
+ (void)_accessibilityInitialize;
+ (void)_preventDockConnections;

- (void)accessibilitySetEnhancedUserInterfaceAttribute:(id)value;
- (id)accessibilityEnhancedUserInterfaceAttribute;

- (void)_updateCanQuitQuietlyAndSafely;

// Conditionally define this only for !USE_APPLE_INTERNAL_SDK when <rdar://problem/63864711> is fixed.
@property (copy, setter=_setAccentColor:) NSColor *_accentColor;
@property (readonly, copy) NSColor *_effectiveAccentColor;

@end

#endif // USE(APPKIT)
