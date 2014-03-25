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

#import "config.h"
#import "WKWindowFeaturesInternal.h"

#if WK_API_ENABLED

#import <WebCore/WindowFeatures.h>
#import <wtf/RetainPtr.h>

@implementation WKWindowFeatures {
    RetainPtr<NSNumber> _menuBarVisibility;
    RetainPtr<NSNumber> _statusBarVisibility;
    RetainPtr<NSNumber> _toolbarsVisibility;

    RetainPtr<NSNumber> _allowsResizing;

    RetainPtr<NSNumber> _x;
    RetainPtr<NSNumber> _y;
    RetainPtr<NSNumber> _width;
    RetainPtr<NSNumber> _height;
}

- (instancetype)_initWithWindowFeatures:(const WebCore::WindowFeatures&)windowFeatures
{
    if (!(self = [super init]))
        return nil;

    // FIXME: These should be set to nil if the features didn't exist in the string.
    _menuBarVisibility = @(windowFeatures.menuBarVisible);
    _statusBarVisibility = @(windowFeatures.statusBarVisible);
    _toolbarsVisibility = @(windowFeatures.toolBarVisible || windowFeatures.locationBarVisible);
    _allowsResizing = @(windowFeatures.resizable);

    if (windowFeatures.xSet)
        _x = @(windowFeatures.x);
    if (windowFeatures.ySet)
        _y = @(windowFeatures.y);
    if (windowFeatures.widthSet)
        _width = @(windowFeatures.width);
    if (windowFeatures.heightSet)
        _height = @(windowFeatures.height);

    return self;
}

- (NSNumber *)menuBarVisibility
{
    return _menuBarVisibility.get();
}

- (NSNumber *)statusBarVisibility
{
    return _statusBarVisibility.get();
}

- (NSNumber *)toolbarsVisibility
{
    return _toolbarsVisibility.get();
}

- (NSNumber *)allowsResizing
{
    return _allowsResizing.get();
}

- (NSNumber *)x
{
    return _x.get();
}

- (NSNumber *)y
{
    return _y.get();
}

- (NSNumber *)width
{
    return _width.get();
}

- (NSNumber *)height
{
    return _height.get();
}

@end

#endif
