/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "RemoteLayerTreeLayers.h"

#if PLATFORM(COCOA)

#import "Logging.h"
#import "RemoteLayerTreeNode.h"
#import <WebCore/DynamicContentScalingDisplayList.h>
#import <WebCore/DynamicContentScalingTypes.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/MachSendRight.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
#import <CoreRE/RECGCommandsContext.h>
#endif

@implementation WKCompositingLayer {
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    RetainPtr<CFDataRef> _displayListDataForTesting;
#endif
}

- (NSString *)description
{
    return WebKit::RemoteLayerTreeNode::appendLayerDescription(super.description, self);
}

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)

- (void)_setWKContents:(id)contents withDisplayList:(WebCore::DynamicContentScalingDisplayList&&)displayList replayForTesting:(BOOL)replay
{
    auto data = displayList.displayList()->createCFData();

    if (replay) {
        _displayListDataForTesting = data;
        [self setNeedsDisplay];
        return;
    }

    self.contents = contents;

    auto surfaces = displayList.takeSurfaces();
    auto ports = adoptNS([[NSMutableArray alloc] initWithCapacity:surfaces.size()]);
    for (MachSendRight& surface : surfaces) {
        // We `leakSendRight` because CAMachPortCreate "adopts" the incoming reference.
        RetainPtr portWrapper = adoptCF(CAMachPortCreate(surface.leakSendRight()));
        [ports addObject:static_cast<id>(portWrapper.get())];
    }

    [self setValue:bridge_cast(data.get()) forKeyPath:WKDynamicContentScalingContentsKey];
    [self setValue:ports.get() forKeyPath:WKDynamicContentScalingPortsKey];
    [self setNeedsDisplay];
}

- (void)drawInContext:(CGContextRef)context
{
    if (!_displayListDataForTesting)
        return;
    CGContextScaleCTM(context, 1, -1);
    CGContextTranslateCTM(context, 0, -self.bounds.size.height);
    RECGContextDrawCGCommandsEncodedData(context, _displayListDataForTesting.get(), nullptr);
}

#endif // ENABLE(RE_DYNAMIC_CONTENT_SCALING)

@end

#endif // PLATFORM(COCOA)
