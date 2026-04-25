/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

#import "WKWebExtensionTabConfiguration.h"

WK_HEADER_AUDIT_BEGIN(nullability, sendability)

#if ENABLE(WK_WEB_EXTENSIONS)

@interface WKWebExtensionTabConfiguration ()

- (instancetype)_init NS_DESIGNATED_INITIALIZER;

@property (readwrite, setter=_setWindow:) id <WKWebExtensionWindow> window;
@property (readwrite, setter=_setIndex:) NSUInteger index;
@property (readwrite, setter=_setParentTab:) id <WKWebExtensionTab> parentTab;
@property (readwrite, setter=_setURL:) NSURL *url;
@property (readwrite, setter=_setShouldBeActive:) BOOL shouldBeActive;
@property (readwrite, setter=_setShouldAddToSelection:) BOOL shouldAddToSelection;
@property (readwrite, setter=_setShouldBePinned:) BOOL shouldBePinned;
@property (readwrite, setter=_setShouldBeMuted:) BOOL shouldBeMuted;
@property (readwrite, setter=_setShouldReaderModeBeActive:) BOOL shouldReaderModeBeActive;

@end

#endif // ENABLE(WK_WEB_EXTENSIONS)

WK_HEADER_AUDIT_END(nullability, sendability)
