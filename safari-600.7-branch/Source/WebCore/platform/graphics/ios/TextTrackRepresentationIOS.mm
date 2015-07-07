/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS) && ENABLE(VIDEO_TRACK)

#include "TextTrackRepresentationIOS.h"

#include "FloatRect.h"
#include "GraphicsContextCG.h"
#include "WebCoreThread.h"
#include "WebCoreThreadRun.h"
#include <wtf/OwnPtr.h>

using namespace WebCore;

@interface WebCoreTextTrackRepresentationIOSHelper : NSObject {
    TextTrackRepresentationIOS* _parent;
}
- (id)initWithParent:(TextTrackRepresentationIOS*)parent;
@property (assign) TextTrackRepresentationIOS* parent;
@end

@implementation WebCoreTextTrackRepresentationIOSHelper
- (id)initWithParent:(TextTrackRepresentationIOS*)parent
{
    if (!(self = [super init]))
        return nil;

    self.parent = parent;

    return self;
}

- (void)dealloc
{
    self.parent = nil;
    [super dealloc];
}

- (void)setParent:(TextTrackRepresentationIOS*)parent
{
    if (_parent)
        [_parent->platformLayer() removeObserver:self forKeyPath:@"bounds"];

    _parent = parent;

    if (_parent)
        [_parent->platformLayer() addObserver:self forKeyPath:@"bounds" options:0 context:0];
}

- (TextTrackRepresentationIOS*)parent
{
    return _parent;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    UNUSED_PARAM(change);
    UNUSED_PARAM(context);
    WebThreadRun(^{
        if (_parent && [keyPath isEqual:@"bounds"] && object == _parent->platformLayer())
            _parent->client()->textTrackRepresentationBoundsChanged(_parent->bounds());
    });
}

- (id)actionForLayer:(CALayer *)layer forKey:(NSString *)event
{
    UNUSED_PARAM(layer);
    UNUSED_PARAM(event);
    // Returning a NSNull from this delegate method disables all implicit CALayer actions.
    return [NSNull null];
}

@end

PassOwnPtr<TextTrackRepresentation> TextTrackRepresentation::create(TextTrackRepresentationClient* client)
{
    return adoptPtr(new TextTrackRepresentationIOS(client));
}

TextTrackRepresentationIOS::TextTrackRepresentationIOS(TextTrackRepresentationClient* client)
    : m_client(client)
    , m_layer(adoptNS([[CALayer alloc] init]))
    , m_delegate(adoptNS([[WebCoreTextTrackRepresentationIOSHelper alloc] initWithParent:this]))
{
    [m_layer.get() setDelegate:m_delegate.get()];
    [m_layer.get() setContentsGravity:kCAGravityBottom];
}

TextTrackRepresentationIOS::~TextTrackRepresentationIOS()
{
    [m_layer.get() setDelegate:nil];
    [m_delegate.get() setParent:0];
}

void TextTrackRepresentationIOS::update()
{
    if (RefPtr<Image> representation = m_client->createTextTrackRepresentationImage())
        [m_layer.get() setContents:(id)representation->getCGImageRef()];
}

void TextTrackRepresentationIOS::setContentScale(float scale)
{
    [m_layer.get() setContentsScale:scale];
}

IntRect TextTrackRepresentationIOS::bounds() const
{
    return enclosingIntRect(FloatRect([m_layer.get() bounds]));
}

#endif // PLATFORM(IOS) && ENABLE(VIDEO_TRACK)
