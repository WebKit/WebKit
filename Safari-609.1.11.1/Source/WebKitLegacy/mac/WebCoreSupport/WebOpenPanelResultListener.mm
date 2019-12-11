/*
 * Copyright (C) 2008 Apple Inc.  All rights reserved.
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

#import "WebOpenPanelResultListener.h"

#import <WebCore/FileChooser.h>
#import <wtf/RefPtr.h>

#if PLATFORM(IOS_FAMILY)
#import <WebCore/Icon.h>
#endif

using namespace WebCore;

@implementation WebOpenPanelResultListener

- (id)initWithChooser:(FileChooser&)chooser
{
    self = [super init];
    if (!self)
        return nil;
    _chooser = &chooser;
    return self;
}

- (void)cancel
{
    _chooser = nullptr;
}

- (void)chooseFilename:(NSString *)filename
{
    ASSERT(_chooser);
    if (!_chooser)
        return;
    _chooser->chooseFile(filename);
    _chooser = nullptr;
}

- (void)chooseFilenames:(NSArray *)filenames
{
    ASSERT(_chooser);
    if (!_chooser)
        return;
    NSUInteger count = [filenames count];
    Vector<String> names(count);
    for (NSUInteger i = 0; i < count; i++)
        names[i] = [filenames objectAtIndex:i];
    _chooser->chooseFiles(names);
    _chooser = nullptr;
}

#if PLATFORM(IOS_FAMILY)

- (void)chooseFilename:(NSString *)filename displayString:(NSString *)displayString iconImage:(CGImageRef)imageRef
{
    [self chooseFilenames:[NSArray arrayWithObject:filename] displayString:displayString iconImage:imageRef];
}

- (void)chooseFilenames:(NSArray *)filenames displayString:(NSString *)displayString iconImage:(CGImageRef)imageRef
{
    ASSERT(_chooser);
    if (!_chooser)
        return;

    Vector<String> names;
    names.reserveInitialCapacity([filenames count]);
    for (NSString *filename in filenames)
        names.uncheckedAppend(filename);
    _chooser->chooseMediaFiles(names, displayString, Icon::createIconForImage(imageRef).get());
    _chooser = nullptr;
}

#endif

@end
