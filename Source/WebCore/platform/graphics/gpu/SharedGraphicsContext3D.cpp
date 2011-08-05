/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "SharedGraphicsContext3D.h"

namespace WebCore {

SharedGraphicsContext3D* SharedGraphicsContext3D::s_instance = 0;

PassRefPtr<SharedGraphicsContext3D> SharedGraphicsContext3D::create(HostWindow* window)
{
    if (s_instance) {
        s_instance->ref(); // Manually increment refcount for this caller since adoptRef() does not increase the refcount.
        return adoptRef(s_instance);
    }
    GraphicsContext3D::Attributes attributes;
    attributes.depth = false;
    attributes.stencil = true;
    attributes.antialias = false;
    attributes.canRecoverFromContextLoss = false; // Canvas contexts can not handle lost contexts.
    RefPtr<GraphicsContext3D> context = GraphicsContext3D::create(attributes, window);
    if (!context)
        return PassRefPtr<SharedGraphicsContext3D>();
    RefPtr<SharedGraphicsContext3D> sharedContext = adoptRef(new SharedGraphicsContext3D(context.release()));
    s_instance = sharedContext.get();
    return sharedContext;
}

SharedGraphicsContext3D::SharedGraphicsContext3D(PassRefPtr<GraphicsContext3D> context)
    : m_context(context)
{
}

SharedGraphicsContext3D::~SharedGraphicsContext3D()
{
    s_instance = 0;
}

}

