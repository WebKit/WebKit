/*
 * Copyright 2012 Adobe Systems Incorporated. All Rights Reserved.
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

#ifndef CustomFilterParameter_h
#define CustomFilterParameter_h

#if ENABLE(CSS_SHADERS)
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CustomFilterParameter : public RefCounted<CustomFilterParameter> {
public:
    // FIXME: Implement other parameters types:
    // booleans: https://bugs.webkit.org/show_bug.cgi?id=76438
    // textures: https://bugs.webkit.org/show_bug.cgi?id=71442
    // 3d-transforms: https://bugs.webkit.org/show_bug.cgi?id=71443
    // mat2, mat3, mat4: https://bugs.webkit.org/show_bug.cgi?id=71444
    enum ParameterType {
        NUMBER
    };
    
    virtual ~CustomFilterParameter() { }
    
    ParameterType parameterType() const { return m_type; }
    const String& name() const { return m_name; }

protected:
    CustomFilterParameter(ParameterType type, const String& name)
        : m_name(name)
        , m_type(type)
    {
    }

private:
    String m_name;
    ParameterType m_type;
};

} // namespace WebCore

#endif // ENABLE(CSS_SHADERS)

#endif // CustomFilterParameter_h
