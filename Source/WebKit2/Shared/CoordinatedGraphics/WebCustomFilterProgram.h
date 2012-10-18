/*
 * Copyright (C) 2012 Company 100, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebCustomFilterProgram_h
#define WebCustomFilterProgram_h

#if USE(COORDINATED_GRAPHICS) && ENABLE(CSS_SHADERS)
#include <WebCore/CustomFilterProgram.h>

namespace WebKit {

class WebCustomFilterProgram : public WebCore::CustomFilterProgram {
public:
    static PassRefPtr<WebCustomFilterProgram> create(String vertexShaderString, String m_fragmentShaderString, WebCore::CustomFilterProgramType programType, WebCore::CustomFilterProgramMixSettings mixSettings)
    {
        return adoptRef(new WebCustomFilterProgram(vertexShaderString, m_fragmentShaderString, programType, mixSettings));
    }

    virtual bool isLoaded() const OVERRIDE { return true; }

    virtual bool operator==(const CustomFilterProgram& o) const OVERRIDE
    {
        // The following cast is ugly, but WebCustomFilterProgram is the single implementation of CustomFilterProgram on UI Process.
        const WebCustomFilterProgram* other = static_cast<const WebCustomFilterProgram*>(&o);
        return mixSettings() == other->mixSettings() && m_vertexShaderString == other->vertexShaderString() && m_fragmentShaderString == other->fragmentShaderString();
    }

protected:
    virtual String vertexShaderString() const OVERRIDE { return m_vertexShaderString; }
    virtual String fragmentShaderString() const OVERRIDE { return m_fragmentShaderString; }

    virtual void willHaveClients() OVERRIDE { notifyClients(); }
    virtual void didRemoveLastClient() OVERRIDE { }

private:
    WebCustomFilterProgram(String vertexShaderString, String fragmentShaderString, WebCore::CustomFilterProgramType programType, WebCore::CustomFilterProgramMixSettings mixSettings)
        : WebCore::CustomFilterProgram(programType, mixSettings)
        , m_vertexShaderString(vertexShaderString)
        , m_fragmentShaderString(fragmentShaderString)
    {
    }

    String m_vertexShaderString;
    String m_fragmentShaderString;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS) && ENABLE(CSS_SHADERS)

#endif // WebCustomFilterProgram_h
