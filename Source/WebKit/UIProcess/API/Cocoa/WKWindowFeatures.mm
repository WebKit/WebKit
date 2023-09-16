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

#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WindowFeatures.h>
#import <wtf/RetainPtr.h>

@implementation WKWindowFeatures

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKWindowFeatures.class, self))
        return;

    _windowFeatures->API::WindowFeatures::~WindowFeatures();

    [super dealloc];
}

- (NSNumber *)menuBarVisibility
{
    if (auto menuBarVisible = _windowFeatures->windowFeatures().menuBarVisible)
        return @(*menuBarVisible);

    return nil;
}

- (NSNumber *)statusBarVisibility
{
    if (auto statusBarVisible = _windowFeatures->windowFeatures().statusBarVisible)
        return @(*statusBarVisible);

    return nil;
}

- (NSNumber *)toolbarsVisibility
{
    if (auto toolBarVisible = _windowFeatures->windowFeatures().toolBarVisible)
        return @(*toolBarVisible);

    return nil;
}

- (NSNumber *)allowsResizing
{
    if (auto resizable = _windowFeatures->windowFeatures().resizable)
        return @(*resizable);

    return nil;
}

- (NSNumber *)x
{
    if (auto x = _windowFeatures->windowFeatures().x)
        return @(*x);

    return nil;
}

- (NSNumber *)y
{
    if (auto y = _windowFeatures->windowFeatures().y)
        return @(*y);

    return nil;
}

- (NSNumber *)width
{
    if (auto width = _windowFeatures->windowFeatures().width)
        return @(*width);

    return nil;
}

- (NSNumber *)height
{
    if (auto height = _windowFeatures->windowFeatures().height)
        return @(*height);

    return nil;
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_windowFeatures;
}

@end

@implementation WKWindowFeatures (WKPrivate)

- (BOOL)_wantsPopup
{
    return _windowFeatures->windowFeatures().wantsPopup();
}

- (BOOL)_hasAdditionalFeatures
{
    return _windowFeatures->windowFeatures().hasAdditionalFeatures;
}

- (NSNumber *)_popup
{
    if (auto popup = _windowFeatures->windowFeatures().popup)
        return @(*popup);

    return nil;
}

- (NSNumber *)_locationBarVisibility
{
    if (auto locationBarVisible = _windowFeatures->windowFeatures().locationBarVisible)
        return @(*locationBarVisible);

    return nil;
}

- (NSNumber *)_scrollbarsVisibility
{
    if (auto scrollbarsVisible = _windowFeatures->windowFeatures().scrollbarsVisible)
        return @(*scrollbarsVisible);

    return nil;
}

- (NSNumber *)_fullscreenDisplay
{
    if (auto fullscreen = _windowFeatures->windowFeatures().fullscreen)
        return @(*fullscreen);

    return nil;
}

- (NSNumber *)_dialogDisplay
{
    if (auto dialog = _windowFeatures->windowFeatures().dialog)
        return @(*dialog);

    return nil;
}

@end
