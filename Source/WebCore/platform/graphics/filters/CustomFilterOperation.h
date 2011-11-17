/*
 * Copyright 2011 Adobe Systems Incorporated. All Rights Reserved.
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

#ifndef CustomFilterOperation_h
#define CustomFilterOperation_h

#if ENABLE(CSS_SHADERS)

#include "FilterOperation.h"
#include "StyleShader.h"

namespace WebCore {

// CSS Shaders

class CustomFilterOperation : public FilterOperation {
public:
    static PassRefPtr<CustomFilterOperation> create(PassRefPtr<StyleShader> vertexShader, PassRefPtr<StyleShader> fragmentShader)
    {
        return adoptRef(new CustomFilterOperation(vertexShader, fragmentShader));
    }
    
    void setVertexShader(PassRefPtr<StyleShader> shader) { m_vertexShader = shader; }
    StyleShader* vertexShader() const { return m_vertexShader.get(); }
    
    void setFragmentShader(PassRefPtr<StyleShader> shader) { m_fragmentShader = shader; }
    StyleShader* fragmentShader() const { return m_fragmentShader.get(); }
    
private:
    virtual bool operator==(const FilterOperation& o) const
    {
        if (!isSameType(o))
            return false;

        const CustomFilterOperation* other = static_cast<const CustomFilterOperation*>(&o);
        return m_vertexShader.get() == other->m_vertexShader.get()
            && m_fragmentShader.get() == other->m_fragmentShader.get();
    }
    
    CustomFilterOperation(PassRefPtr<StyleShader> vertexShader, PassRefPtr<StyleShader> fragmentShader)
        : FilterOperation(CUSTOM)
        , m_vertexShader(vertexShader)
        , m_fragmentShader(fragmentShader)
    {
    }

    RefPtr<StyleShader> m_vertexShader;
    RefPtr<StyleShader> m_fragmentShader;
};

} // namespace WebCore

#endif // ENABLE(CSS_SHADERS)

#endif // CustomFilterOperation_h
