/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
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

#ifndef StyleCustomFilterProgram_h
#define StyleCustomFilterProgram_h

#if ENABLE(CSS_SHADERS)
#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include "CachedShader.h"
#include "CustomFilterProgram.h"
#include "StyleShader.h"
#include <wtf/FastAllocBase.h>

namespace WebCore {

// CSS Shaders

class StyleCustomFilterProgram : public CustomFilterProgram, public CachedResourceClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassRefPtr<StyleCustomFilterProgram> create(PassRefPtr<StyleShader> vertexShader, PassRefPtr<StyleShader> fragmentShader)
    {
        return adoptRef(new StyleCustomFilterProgram(vertexShader, fragmentShader));
    }
    
    void setVertexShader(PassRefPtr<StyleShader> shader) { m_vertexShader = shader; }
    StyleShader* vertexShader() const { return m_vertexShader.get(); }
    
    void setFragmentShader(PassRefPtr<StyleShader> shader) { m_fragmentShader = shader; }
    StyleShader* fragmentShader() const { return m_fragmentShader.get(); }
    
    virtual String vertexShaderString() const
    {
        ASSERT(isLoaded());
        return m_cachedVertexShader.get() ? m_cachedVertexShader->shaderString() : String();
    }
    
    virtual String fragmentShaderString() const
    {
        ASSERT(isLoaded());
        return m_cachedFragmentShader.get() ? m_cachedFragmentShader->shaderString() : String();
    }

    virtual bool isLoaded() const
    {
        // Do not use the CachedResource:isLoaded method here, because it actually means !isLoading(),
        // so missing and canceled resources will have isLoaded set to true, even if they are not loaded yet.
        return (!m_cachedVertexShader.get() || m_isVertexShaderLoaded)
            && (!m_cachedFragmentShader.get() || m_isFragmentShaderLoaded);
    }

    virtual void willHaveClients()
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
    
    virtual void didRemoveLastClient()
    {
        if (m_cachedVertexShader.get()) {
            m_cachedVertexShader->removeClient(this);
            m_cachedVertexShader = 0;
            m_isVertexShaderLoaded = false;
        }
        if (m_cachedFragmentShader.get()) {
            m_cachedFragmentShader->removeClient(this);
            m_cachedFragmentShader = 0;
            m_isFragmentShaderLoaded = false;
        }
    }
    
    virtual void notifyFinished(CachedResource* resource)
    {
        if (resource->errorOccurred())
            return;
        if (resource == m_cachedVertexShader.get())
            m_isVertexShaderLoaded = true;
        else if (resource == m_cachedFragmentShader.get())
            m_isFragmentShaderLoaded = true;
        if (isLoaded())
            notifyClients();
    }
    
    CachedShader* cachedVertexShader() const { return m_vertexShader ? m_vertexShader->cachedShader() : 0; }
    CachedShader* cachedFragmentShader() const { return m_fragmentShader ? m_fragmentShader->cachedShader() : 0; }
    
    virtual bool operator==(const CustomFilterProgram& o) const 
    {
        // The following cast is ugly, but StyleCustomFilterProgram is the single implementation of CustomFilterProgram.
        const StyleCustomFilterProgram* other = static_cast<const StyleCustomFilterProgram*>(&o);
        return cachedVertexShader() == other->cachedVertexShader() && cachedFragmentShader() == other->cachedFragmentShader();
    }

private:
    StyleCustomFilterProgram(PassRefPtr<StyleShader> vertexShader, PassRefPtr<StyleShader> fragmentShader)
        : m_vertexShader(vertexShader)
        , m_fragmentShader(fragmentShader)
        , m_isVertexShaderLoaded(false)
        , m_isFragmentShaderLoaded(false)
    {
    }
    
    RefPtr<StyleShader> m_vertexShader;
    RefPtr<StyleShader> m_fragmentShader;
    
    CachedResourceHandle<CachedShader> m_cachedVertexShader;
    CachedResourceHandle<CachedShader> m_cachedFragmentShader;
    
    bool m_isVertexShaderLoaded;
    bool m_isFragmentShaderLoaded;
};

} // namespace WebCore

#endif // ENABLE(CSS_SHADERS)

#endif // StyleCustomFilterProgram_h
