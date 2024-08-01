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

#import "WKWebExtensionWindowCreationOptions.h"

#if ENABLE(WK_WEB_EXTENSIONS)

@interface WKWebExtensionWindowCreationOptions ()

- (instancetype)_init NS_DESIGNATED_INITIALIZER;

@property (readwrite, setter=_setFrame:) CGRect frame;
@property (readwrite, setter=_setWindowType:) WKWebExtensionWindowType windowType;
@property (readwrite, setter=_setWindowState:) WKWebExtensionWindowState windowState;
@property (readwrite, setter=_setTabURLs:) NSArray<NSURL *> *tabURLs;
@property (readwrite, setter=_setTabs:) NSArray<id <WKWebExtensionTab>> *tabs;
@property (readwrite, setter=_setFocused:) BOOL focused;
@property (readwrite, setter=_setUsePrivateBrowsing:) BOOL usePrivateBrowsing;

@end

#endif // ENABLE(WK_WEB_EXTENSIONS)
