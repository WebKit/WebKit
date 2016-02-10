/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ContentSecurityPolicySourceList_h
#define ContentSecurityPolicySourceList_h

#include "ContentSecurityPolicySource.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ContentSecurityPolicy;
class URL;

class ContentSecurityPolicySourceList {
public:
    ContentSecurityPolicySourceList(const ContentSecurityPolicy&, const String& directiveName);

    void parse(const String&);
    bool matches(const URL&);
    bool allowInline() const { return m_allowInline; }
    bool allowEval() const { return m_allowEval; }
    bool allowSelf() const { return m_allowSelf; }

private:
    void parse(const UChar* begin, const UChar* end);

    bool parseSource(const UChar* begin, const UChar* end, String& scheme, String& host, int& port, String& path, bool& hostHasWildcard, bool& portHasWildcard);
    bool parseScheme(const UChar* begin, const UChar* end, String& scheme);
    bool parseHost(const UChar* begin, const UChar* end, String& host, bool& hostHasWildcard);
    bool parsePort(const UChar* begin, const UChar* end, int& port, bool& portHasWildcard);
    bool parsePath(const UChar* begin, const UChar* end, String& path);

    const ContentSecurityPolicy& m_policy;
    Vector<ContentSecurityPolicySource> m_list;
    String m_directiveName;
    bool m_allowSelf { false };
    bool m_allowStar { false };
    bool m_allowInline { false };
    bool m_allowEval { false };
};

} // namespace WebCore

#endif /* ContentSecurityPolicySourceList_h */
