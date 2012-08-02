/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <public/WebSharedGraphicsContext3D.h>

#include "GraphicsContext3DPrivate.h"
#include "SharedGraphicsContext3D.h"

using WebCore::GraphicsContext3DPrivate;
using WebCore::SharedGraphicsContext3D;

namespace WebKit {

WebGraphicsContext3D* WebSharedGraphicsContext3D::mainThreadContext()
{
    return GraphicsContext3DPrivate::extractWebGraphicsContext3D(SharedGraphicsContext3D::get().get());
}

GrContext* WebSharedGraphicsContext3D::mainThreadGrContext()
{
    return SharedGraphicsContext3D::get() ? SharedGraphicsContext3D::get()->grContext() : 0;
}


WebGraphicsContext3D* WebSharedGraphicsContext3D::compositorThreadContext()
{
    return GraphicsContext3DPrivate::extractWebGraphicsContext3D(SharedGraphicsContext3D::getForImplThread().get());
}

GrContext* WebSharedGraphicsContext3D::compositorThreadGrContext()
{
    return SharedGraphicsContext3D::getForImplThread() ? SharedGraphicsContext3D::getForImplThread()->grContext() : 0;
}

bool WebSharedGraphicsContext3D::haveCompositorThreadContext()
{
    return SharedGraphicsContext3D::haveForImplThread();
}

bool WebSharedGraphicsContext3D::createCompositorThreadContext()
{
    return SharedGraphicsContext3D::createForImplThread();
}

}

