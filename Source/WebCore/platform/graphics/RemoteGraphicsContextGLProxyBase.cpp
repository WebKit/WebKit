/*
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteGraphicsContextGLProxyBase.h"

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL)
#include "NotImplemented.h"

namespace WebCore {

RemoteGraphicsContextGLProxyBase::RemoteGraphicsContextGLProxyBase(const GraphicsContextGLAttributes& attrs)
    : GraphicsContextGL(attrs)
{
    platformInitialize();
}

RemoteGraphicsContextGLProxyBase::~RemoteGraphicsContextGLProxyBase() = default;

ExtensionsGL& RemoteGraphicsContextGLProxyBase::getExtensions()
{
    return *this;
}

void RemoteGraphicsContextGLProxyBase::setContextVisibility(bool)
{
    notImplemented();
}

bool RemoteGraphicsContextGLProxyBase::isGLES2Compliant() const
{
#if ENABLE(WEBGL2)
    return contextAttributes().webGLVersion == GraphicsContextGLWebGLVersion::WebGL2;
#else
    return false;
#endif
}

void RemoteGraphicsContextGLProxyBase::markContextChanged()
{
    // FIXME: The caller should track this state.
    if (m_layerComposited)
        notifyMarkContextChanged();
    m_layerComposited = false;
}

void RemoteGraphicsContextGLProxyBase::markLayerComposited()
{
    m_layerComposited = true;
    auto attrs = contextAttributes();
    if (!attrs.preserveDrawingBuffer) {
        m_buffersToAutoClear = GraphicsContextGL::COLOR_BUFFER_BIT;
        if (attrs.depth)
            m_buffersToAutoClear |= GraphicsContextGL::DEPTH_BUFFER_BIT;
        if (attrs.stencil)
            m_buffersToAutoClear |= GraphicsContextGL::STENCIL_BUFFER_BIT;
    }
    for (auto* client : copyToVector(m_clients))
        client->didComposite();
}

bool RemoteGraphicsContextGLProxyBase::layerComposited() const
{
    return m_layerComposited;
}

void RemoteGraphicsContextGLProxyBase::setBuffersToAutoClear(GCGLbitfield buffers)
{
    if (!contextAttributes().preserveDrawingBuffer)
        m_buffersToAutoClear = buffers;
}

GCGLbitfield RemoteGraphicsContextGLProxyBase::getBuffersToAutoClear() const
{
    return m_buffersToAutoClear;
}

void RemoteGraphicsContextGLProxyBase::enablePreserveDrawingBuffer()
{
    // Redeclared for export reasons.
    // FIXME: The whole function should be removed.
    GraphicsContextGL::enablePreserveDrawingBuffer();
}

bool RemoteGraphicsContextGLProxyBase::supports(const String& name)
{
    waitUntilInitialized();
    return m_availableExtensions.contains(name) || m_requestableExtensions.contains(name);
}

void RemoteGraphicsContextGLProxyBase::ensureEnabled(const String& name)
{
    waitUntilInitialized();
    if (m_requestableExtensions.contains(name) && !m_enabledExtensions.contains(name)) {
        ensureExtensionEnabled(name);
        m_enabledExtensions.add(name);
    }
}

bool RemoteGraphicsContextGLProxyBase::isEnabled(const String& name)
{
    waitUntilInitialized();
    return m_availableExtensions.contains(name) || m_enabledExtensions.contains(name);
}

void RemoteGraphicsContextGLProxyBase::initialize(const String& availableExtensions, const String& requestableExtensions)
{
    for (auto& extension : availableExtensions.split(' '))
        m_availableExtensions.add(extension);
    for (auto& extension : requestableExtensions.split(' '))
        m_requestableExtensions.add(extension);
}

#if !PLATFORM(COCOA)
void RemoteGraphicsContextGLProxyBase::platformInitialize()
{
}

PlatformLayer* RemoteGraphicsContextGLProxyBase::platformLayer() const
{
    return nullptr;
}
#endif
#if !USE(ANGLE)
void RemoteGraphicsContextGLProxyBase::readnPixelsEXT(GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLenum, GCGLenum, GCGLsizei, GCGLvoid*)
{
}

void RemoteGraphicsContextGLProxyBase::getnUniformfvEXT(GCGLuint, GCGLint, GCGLsizei, GCGLfloat*)
{
}

void RemoteGraphicsContextGLProxyBase::getnUniformivEXT(GCGLuint, GCGLint, GCGLsizei, GCGLint*)
{
}

bool RemoteGraphicsContextGLProxyBase::isNVIDIA()
{
    return false;
}

bool RemoteGraphicsContextGLProxyBase::isAMD()
{
    return false;
}

bool RemoteGraphicsContextGLProxyBase::isIntel()
{
    return false;
}

bool RemoteGraphicsContextGLProxyBase::isImagination()
{
    return false;
}

String RemoteGraphicsContextGLProxyBase::vendor()
{
    return { };
}

bool RemoteGraphicsContextGLProxyBase::requiresBuiltInFunctionEmulation()
{
    return false;
}

bool RemoteGraphicsContextGLProxyBase::requiresRestrictedMaximumTextureSize()
{
    return false;
}
#endif
}
#endif
