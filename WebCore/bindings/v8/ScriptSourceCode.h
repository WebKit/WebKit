/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScriptSourceCode_h
#define ScriptSourceCode_h

#include "CachedScript.h"
#include "KURL.h"
#include "PlatformString.h"

namespace WebCore {

class ScriptSourceCode {
public:
    ScriptSourceCode(const String& source, const KURL& url = KURL(), int startLine = 1)
        : m_source(source)
        , m_url(url)
        , m_startLine(startLine)
    {
    }

    // We lose the encoding information from CachedScript.
    // Not sure if that matters.
    ScriptSourceCode(CachedScript* cs)
        : m_source(cs->script())
        , m_url(cs->url())
        , m_startLine(1)
    {
    }

    bool isEmpty() const { return m_source.isEmpty(); }

    const String& source() const { return m_source; }
    const KURL& url() const { return m_url; }
    int startLine() const { return m_startLine; }

private:
    String m_source;
    KURL m_url;
    int m_startLine;
};

} // namespace WebCore

#endif // ScriptSourceCode_h
