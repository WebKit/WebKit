/*
 * Copyright (C) 2011 Adam Barth. All Rights Reserved.
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

#pragma once

#include "HTMLToken.h"
#include "HTTPParsers.h"
#include "URL.h"
#include "SuffixTree.h"
#include "TextEncoding.h"

namespace WebCore {

class Document;
class HTMLDocumentParser;
class HTMLSourceTracker;
class XSSInfo;
class XSSAuditorDelegate;

struct FilterTokenRequest {
    FilterTokenRequest(HTMLToken& token, HTMLSourceTracker& sourceTracker, bool shouldAllowCDATA)
        : token(token)
        , sourceTracker(sourceTracker)
        , shouldAllowCDATA(shouldAllowCDATA)
    { }

    HTMLToken& token;
    HTMLSourceTracker& sourceTracker;
    bool shouldAllowCDATA;
};

class XSSAuditor {
    WTF_MAKE_NONCOPYABLE(XSSAuditor);
public:
    XSSAuditor();

    void init(Document*, XSSAuditorDelegate*);
    void initForFragment();

    std::unique_ptr<XSSInfo> filterToken(const FilterTokenRequest&);

private:
    static const size_t kMaximumFragmentLengthTarget = 100;

    enum State {
        Uninitialized,
        Initialized
    };

    enum class TruncationStyle {
        None,
        NormalAttribute,
        SrcLikeAttribute,
        ScriptLikeAttribute
    };

    bool filterStartToken(const FilterTokenRequest&);
    void filterEndToken(const FilterTokenRequest&);
    bool filterCharacterToken(const FilterTokenRequest&);
    bool filterScriptToken(const FilterTokenRequest&);
    bool filterObjectToken(const FilterTokenRequest&);
    bool filterParamToken(const FilterTokenRequest&);
    bool filterEmbedToken(const FilterTokenRequest&);
    bool filterAppletToken(const FilterTokenRequest&);
    bool filterFrameToken(const FilterTokenRequest&);
    bool filterMetaToken(const FilterTokenRequest&);
    bool filterBaseToken(const FilterTokenRequest&);
    bool filterFormToken(const FilterTokenRequest&);
    bool filterInputToken(const FilterTokenRequest&);
    bool filterButtonToken(const FilterTokenRequest&);

    bool eraseDangerousAttributesIfInjected(const FilterTokenRequest&);
    bool eraseAttributeIfInjected(const FilterTokenRequest&, const QualifiedName&, const String& replacementValue = String(), TruncationStyle = TruncationStyle::NormalAttribute);

    String canonicalizedSnippetForTagName(const FilterTokenRequest&);
    String canonicalizedSnippetForJavaScript(const FilterTokenRequest&);
    String snippetFromAttribute(const FilterTokenRequest&, const HTMLToken::Attribute&);
    String canonicalize(const String&, TruncationStyle);

    bool isContainedInRequest(const String&);
    bool isLikelySafeResource(const String& url);

    SuffixTree<ASCIICodebook>* decodedHTTPBodySuffixTree();

    URL m_documentURL;
    bool m_isEnabled;

    XSSProtectionDisposition m_xssProtection;
    bool m_didSendValidXSSProtectionHeader;

    String m_decodedURL;
    String m_decodedHTTPBody;
    std::unique_ptr<SuffixTree<ASCIICodebook>> m_decodedHTTPBodySuffixTree;

    State m_state;
    bool m_wasScriptTagFoundInRequest { false };
    unsigned m_scriptTagNestingLevel;
    TextEncoding m_encoding;
};

} // namespace WebCore
