/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "StyleCustomFilterProgram.h"

#if ENABLE(CSS_SHADERS)

#include "CachedShader.h"

namespace WebCore {

String StyleCustomFilterProgram::vertexShaderString() const
{
    ASSERT(isLoaded());
    return m_cachedVertexShader ? m_cachedVertexShader->shaderString() : String();
}

String StyleCustomFilterProgram::fragmentShaderString() const
{
    ASSERT(isLoaded());
    return m_cachedFragmentShader ? m_cachedFragmentShader->shaderString() : String();
}

bool StyleCustomFilterProgram::isLoaded() const
{
    // Do not use the CachedResource:isLoaded method here, because it actually means !isLoading(),
    // so missing and canceled resources will have isLoaded set to true, even if they are not loaded yet.
    return (!m_cachedVertexShader || m_isVertexShaderLoaded)
        && (!m_cachedFragmentShader || m_isFragmentShaderLoaded);
}

void StyleCustomFilterProgram::willHaveClients()
{
    if (m_vertexShader) {
        m_cachedVertexShader = m_vertexShader->cachedShader();
        m_cachedVertexShader->addClient(this);
    }
    if (m_fragmentShader) {
        m_cachedFragmentShader = m_fragmentShader->cachedShader();
        m_cachedFragmentShader->addClient(this);
    }
}

void StyleCustomFilterProgram::didRemoveLastClient()
{
    if (m_cachedVertexShader) {
        m_cachedVertexShader->removeClient(this);
        m_cachedVertexShader = 0;
        m_isVertexShaderLoaded = false;
    }
    if (m_cachedFragmentShader) {
        m_cachedFragmentShader->removeClient(this);
        m_cachedFragmentShader = 0;
        m_isFragmentShaderLoaded = false;
    }
}

void StyleCustomFilterProgram::notifyFinished(CachedResource* resource)
{
    if (resource->errorOccurred())
        return;
    // Note that m_cachedVertexShader might be equal to m_cachedFragmentShader and it would only get one event in that case.
    if (resource == m_cachedVertexShader.get())
        m_isVertexShaderLoaded = true;
    if (resource == m_cachedFragmentShader.get())
        m_isFragmentShaderLoaded = true;
    if (isLoaded())
        notifyClients();
}

} // namespace WebCore

#endif // ENABLE(CSS_SHADERS)
