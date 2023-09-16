/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import "_WKWebExtensionTabCreationOptions.h"

NS_ASSUME_NONNULL_BEGIN

#if ENABLE(WK_WEB_EXTENSIONS)

@interface _WKWebExtensionTabCreationOptions ()

- (instancetype)_init NS_DESIGNATED_INITIALIZER;

@property (readwrite, setter=_setDesiredWindow:) id <_WKWebExtensionWindow> desiredWindow;
@property (readwrite, setter=_setDesiredIndex:) NSUInteger desiredIndex;
@property (readwrite, setter=_setDesiredParentTab:) id <_WKWebExtensionTab> desiredParentTab;
@property (readwrite, setter=_setDesiredURL:) NSURL *desiredURL;
@property (readwrite, setter=_setShouldActivate:) BOOL shouldActivate;
@property (readwrite, setter=_setShouldSelect:) BOOL shouldSelect;
@property (readwrite, setter=_setShouldPin:) BOOL shouldPin;
@property (readwrite, setter=_setShouldMute:) BOOL shouldMute;
@property (readwrite, setter=_setShouldShowReaderMode:) BOOL shouldShowReaderMode;

@end

#endif // ENABLE(WK_WEB_EXTENSIONS)

NS_ASSUME_NONNULL_END
