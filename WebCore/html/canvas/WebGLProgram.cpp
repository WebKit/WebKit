/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(3D_CANVAS)

#include "WebGLProgram.h"
#include "WebGLRenderingContext.h"

namespace WebCore {
    
PassRefPtr<WebGLProgram> WebGLProgram::create(WebGLRenderingContext* ctx)
{
    return adoptRef(new WebGLProgram(ctx));
}

WebGLProgram::WebGLProgram(WebGLRenderingContext* ctx)
    : CanvasObject(ctx)
{
    setObject(context()->graphicsContext3D()->createProgram());
}

void WebGLProgram::_deleteObject(Platform3DObject object)
{
    context()->graphicsContext3D()->deleteProgram(object);
}

bool WebGLProgram::cacheActiveAttribLocations()
{
    m_activeAttribLocations.clear();
    if (!object())
        return false;
    GraphicsContext3D* context3d = context()->graphicsContext3D();
    int linkStatus;
    context3d->getProgramiv(this, GraphicsContext3D::LINK_STATUS, &linkStatus);
    if (!linkStatus)
        return false;

    int numAttribs = 0;
    context3d->getProgramiv(this, GraphicsContext3D::ACTIVE_ATTRIBUTES, &numAttribs);
    m_activeAttribLocations.resize(static_cast<size_t>(numAttribs));
    for (int i = 0; i < numAttribs; ++i) {
        ActiveInfo info;
        context3d->getActiveAttrib(this, i, info);
        m_activeAttribLocations[i] = context3d->getAttribLocation(this, info.name.charactersWithNullTermination());
    }

    return true;
}

int WebGLProgram::numActiveAttribLocations()
{
    return static_cast<int>(m_activeAttribLocations.size());
}

int WebGLProgram::getActiveAttribLocation(int index)
{
    if (index < 0 || index >= numActiveAttribLocations())
        return -1;
    return m_activeAttribLocations[static_cast<size_t>(index)];
}

}

#endif // ENABLE(3D_CANVAS)
