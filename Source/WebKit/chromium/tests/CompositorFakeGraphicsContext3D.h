/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef CompositorFakeGraphicsContext3D_h
#define CompositorFakeGraphicsContext3D_h

#include "CompositorFakeWebGraphicsContext3D.h"
#include "GraphicsContext3D.h"
#include "GraphicsContext3DPrivate.h"

namespace WebCore {

static PassRefPtr<GraphicsContext3D> createCompositorMockGraphicsContext3D(GraphicsContext3D::Attributes attrs)
{
    WebKit::WebGraphicsContext3D::Attributes webAttrs;
    webAttrs.alpha = attrs.alpha;

    OwnPtr<WebKit::WebGraphicsContext3D> webContext = WebKit::CompositorFakeWebGraphicsContext3D::create(webAttrs);
    return GraphicsContext3DPrivate::createGraphicsContextFromWebContext(
        webContext.release(), attrs, 0,
        GraphicsContext3D::RenderDirectlyToHostWindow,
        GraphicsContext3DPrivate::ForUseOnAnotherThread);
}

}

#endif
