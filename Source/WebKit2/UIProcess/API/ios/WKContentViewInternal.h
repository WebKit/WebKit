/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#import "WKContentView.h"
#import <wtf/Forward.h>
#import <wtf/Vector.h>

namespace WebCore {
class Color;
class FloatQuad;
class IntSize;
struct ViewportArguments;
}

namespace WebKit {
class DrawingAreaProxy;
class GeolocationPermissionRequestProxy;
class LayerTreeContext;
class RemoteLayerTreeTransaction;
class WebFrameProxy;
class WebPageProxy;
class WebSecurityOrigin;
struct InteractionInformationAtPosition;
}

@class WebIOSEvent;

@interface WKContentView () {
@package
    RefPtr<WebKit::WebPageProxy> _page;
}

- (std::unique_ptr<WebKit::DrawingAreaProxy>)_createDrawingAreaProxy;
- (void)_processDidExit;
- (void)_didRelaunchProcess;
- (void)_setAcceleratedCompositingRootLayer:(CALayer *)rootLayer;

- (void)_didCommitLoadForMainFrame;
- (void)_didCommitLayerTree:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction;

- (void)_didGetTapHighlightForRequest:(uint64_t)requestID color:(const WebCore::Color&)color quads:(const Vector<WebCore::FloatQuad>&)highlightedQuads topLeftRadius:(const WebCore::IntSize&)topLeftRadius topRightRadius:(const WebCore::IntSize&)topRightRadius bottomLeftRadius:(const WebCore::IntSize&)bottomLeftRadius bottomRightRadius:(const WebCore::IntSize&)bottomRightRadius;

- (void)_startAssistingNode;
- (void)_stopAssistingNode;
- (void)_selectionChanged;
- (BOOL)_interpretKeyEvent:(WebIOSEvent *)theEvent isCharEvent:(BOOL)isCharEvent;
- (void)_positionInformationDidChange:(const WebKit::InteractionInformationAtPosition&)info;
- (void)_decidePolicyForGeolocationRequestFromOrigin:(WebKit::WebSecurityOrigin&)origin frame:(WebKit::WebFrameProxy&)frame request:(WebKit::GeolocationPermissionRequestProxy&)permissionRequest;

@end
