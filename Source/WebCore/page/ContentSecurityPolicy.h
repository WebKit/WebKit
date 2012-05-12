/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
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

#ifndef ContentSecurityPolicy_h
#define ContentSecurityPolicy_h

#include <wtf/PassOwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSPDirectiveList;
class ScriptExecutionContext;
class KURL;

typedef Vector<OwnPtr<CSPDirectiveList> > CSPDirectiveListVector;

class ContentSecurityPolicy {
public:
    static PassOwnPtr<ContentSecurityPolicy> create(ScriptExecutionContext* scriptExecutionContext)
    {
        return adoptPtr(new ContentSecurityPolicy(scriptExecutionContext));
    }
    ~ContentSecurityPolicy();

    void copyStateFrom(const ContentSecurityPolicy*);

    enum HeaderType {
        ReportOnly,
        EnforcePolicy
    };

    void didReceiveHeader(const String&, HeaderType);

    // These functions are wrong becuase they assume that there is only one header.
    // FIXME: Replace them with functions that return vectors.
    const String& deprecatedHeader() const;
    HeaderType deprecatedHeaderType() const;

    bool allowJavaScriptURLs() const;
    bool allowInlineEventHandlers() const;
    bool allowInlineScript() const;
    bool allowInlineStyle() const;
    bool allowEval() const;

    bool allowScriptFromSource(const KURL&) const;
    bool allowObjectFromSource(const KURL&) const;
    bool allowChildFrameFromSource(const KURL&) const;
    bool allowImageFromSource(const KURL&) const;
    bool allowStyleFromSource(const KURL&) const;
    bool allowFontFromSource(const KURL&) const;
    bool allowMediaFromSource(const KURL&) const;
    bool allowConnectToSource(const KURL&) const;

    void setOverrideAllowInlineStyle(bool);

private:
    explicit ContentSecurityPolicy(ScriptExecutionContext*);

    ScriptExecutionContext* m_scriptExecutionContext;
    bool m_overrideInlineStyleAllowed;
    CSPDirectiveListVector m_policies;
};

}

#endif
