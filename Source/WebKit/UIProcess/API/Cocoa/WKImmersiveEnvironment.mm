/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "WKImmersiveEnvironmentInternal.h"

#import <wtf/RetainPtr.h>

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
#import "UIKitSPI.h"
#endif

@implementation WKImmersiveEnvironment {
    RetainPtr<WKFrameInfo> _sourceFrame;
#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    RetainPtr<UIView> _environmentView;
    uint64_t _contextID;
    pid_t _processIdentifier;
#endif
}

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)

- (instancetype)_initWithContextID:(WebCore::LayerHostingContextIdentifier)contextID processIdentifier:(pid_t)processIdentifier frameInfo:(WKFrameInfo *)frameInfo
{
    if (!(self = [super init]))
        return nil;

    _sourceFrame = frameInfo;
    _contextID = contextID.toUInt64();
    _processIdentifier = processIdentifier;

    return self;
}

- (UIView *)_environmentView
{
    if (!_environmentView) {
        RetainPtr environmentView = adoptNS([[UIView alloc] initWithFrame:CGRectZero]);
        RetainPtr remoteModelView = adoptNS([[_UIRemoteView alloc] initWithFrame:CGRectZero pid:_processIdentifier contextID:_contextID]);
        [environmentView addSubview:remoteModelView.get()];
        // To match the assumptions made in ModelProcessModelPlayerProxy.mm, the frame of the model view must stay zero, and be centered inside its container.
        // This ensures that the model is correctly placed at the world's origin when the client puts the view inside their Immersive Space.
        [remoteModelView setFrame:CGRectZero];
        [remoteModelView setAutoresizingMask:(UIViewAutoresizingNone | UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleRightMargin | UIViewAutoresizingFlexibleBottomMargin)];
        _environmentView = WTF::move(environmentView);
    }
    return _environmentView.get();
}

#endif

- (WKFrameInfo *)sourceFrame
{
    return _sourceFrame.get();
}

@end
