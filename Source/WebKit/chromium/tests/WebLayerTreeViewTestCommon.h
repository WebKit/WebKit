/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebLayerTreeViewTestCommon_h
#define WebLayerTreeViewTestCommon_h

#include "CompositorFakeWebGraphicsContext3D.h"
#include <gmock/gmock.h>
#include <public/Platform.h>
#include <public/WebCompositorSupport.h>
#include <public/WebLayerTreeViewClient.h>

namespace WebKit {

class MockWebLayerTreeViewClient : public WebLayerTreeViewClient {
public:
    MOCK_METHOD0(scheduleComposite, void());
    virtual void updateAnimations(double frameBeginTime) OVERRIDE { }
    virtual void layout() OVERRIDE { }
    virtual void applyScrollAndScale(const WebSize& scrollDelta, float scaleFactor) OVERRIDE { }

    virtual WebCompositorOutputSurface* createOutputSurface() OVERRIDE
    {
        return Platform::current()->compositorSupport()->createOutputSurfaceFor3D(CompositorFakeWebGraphicsContext3D::create(WebGraphicsContext3D::Attributes()).leakPtr());
    }
    virtual void didRecreateOutputSurface(bool) OVERRIDE { }
};

}

#endif // WebLayerTreeViewTestCommon_h
