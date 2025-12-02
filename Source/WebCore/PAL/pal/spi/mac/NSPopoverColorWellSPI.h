/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

DECLARE_SYSTEM_HEADER

#import <wtf/Platform.h>

#if USE(APPKIT)

#if USE(APPLE_INTERNAL_SDK)
#import <AppKit/NSColorPopoverController_Private.h>
#import <AppKit/NSPopoverColorWell.h>
#else

@interface NSPopoverColorWell : NSColorWell<NSPopoverDelegate>
@end

@interface NSColorPopoverController : NSViewController
@end

@class NSColorPickerMatrixView;

@interface NSColorPopoverController ()
@property (assign) id delegate;
@property (assign) NSPopover *popover;
@end

#endif

@interface NSPopoverColorWell ()
- (void)_showPopover;
@end

@interface NSColorPickerMatrixView : NSView
@end

@interface NSColorPickerMatrixView ()
- (void)setColorList:(NSColorList *)list;
- (void)setSwatchSize:(NSSize)size;
- (void)setNumberOfColumns:(NSUInteger)columns;
@end

@interface NSColorPopoverController ()
@property (assign) NSColorPickerMatrixView *topBarMatrixView;
@end

#endif // USE(APPKIT)
