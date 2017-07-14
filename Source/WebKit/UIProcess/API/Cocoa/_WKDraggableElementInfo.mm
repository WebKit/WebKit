/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "_WKDraggableElementInfoInternal.h"

#if WK_API_ENABLED

@implementation _WKDraggableElementInfo

#if PLATFORM(IOS)

+ (instancetype)infoWithInteractionInformationAtPosition:(const WebKit::InteractionInformationAtPosition&)information
{
    return [[[self alloc] initWithInteractionInformationAtPosition:information] autorelease];
}

- (instancetype)initWithInteractionInformationAtPosition:(const WebKit::InteractionInformationAtPosition&)information
{
    if (!(self = [super init]))
        return nil;

    _point = information.request.point;
    _link = information.isLink;
    _attachment = information.isAttachment;
    _image = information.isImage;
#if ENABLE(DATA_INTERACTION)
    _selected = information.hasSelectionAtPosition;
#endif

    return self;
}

#endif

- (instancetype)copyWithZone:(NSZone *)zone
{
    _WKDraggableElementInfo *copy = [[_WKDraggableElementInfo alloc] init];

    copy.point = self.point;
    copy.link = self.link;
    copy.attachment = self.attachment;
    copy.image = self.image;
    copy.selected = self.selected;

    return copy;
}

@end

#endif
