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

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import <WebCore/GraphicsLayer.h>

namespace WebKit {
class RemoteLayerTreeHost;
class WebPageProxy;
}

@protocol WKNativelyInteractible <NSObject>
@end

@protocol WKContentControlled <NSObject>
@end

@interface WKCompositingView : UIView <WKContentControlled>
@end

@interface WKTransformView : WKCompositingView
@end

@interface WKSimpleBackdropView : WKCompositingView
@end

@interface WKShapeView : WKCompositingView
@end

@interface WKRemoteView : WKCompositingView

- (instancetype)initWithFrame:(CGRect)frame contextID:(uint32_t)contextID;

@end

@interface WKUIRemoteView : _UIRemoteView <WKContentControlled>
@end

@interface WKBackdropView : _UIBackdropView <WKContentControlled>
@end

@interface WKChildScrollView : UIScrollView <WKContentControlled>
@end

namespace WebKit {

OptionSet<WebCore::TouchAction> touchActionsForPoint(UIView *rootView, const WebCore::IntPoint&);
UIScrollView *findActingScrollParent(UIScrollView *, const RemoteLayerTreeHost&);

#if ENABLE(EDITABLE_REGION)
bool mayContainEditableElementsInRect(UIView *rootView, const WebCore::FloatRect&);
#endif

}

#endif // PLATFORM(IOS_FAMILY)
