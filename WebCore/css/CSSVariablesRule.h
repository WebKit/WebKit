/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#ifndef CSSVariablesRule_h
#define CSSVariablesRule_h

#include "CSSRule.h"
#include "CSSVariablesDeclaration.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSStyleSheet;
class MediaList;

class CSSVariablesRule : public CSSRule {
public:
    static PassRefPtr<CSSVariablesRule> create(CSSStyleSheet* parent, MediaList* mediaList, bool variablesKeyword)
    {
        return adoptRef(new CSSVariablesRule(parent, mediaList, variablesKeyword));
    }

    virtual ~CSSVariablesRule();

    // CSSVariablesRule interface
    MediaList* media() const { return m_lstMedia.get(); }
    CSSVariablesDeclaration* variables() { return m_variables.get(); }

    // Inherited from CSSRule
    virtual unsigned short type() const { return VARIABLES_RULE; }
    virtual String cssText() const;
    virtual bool isVariablesRule() { return true; }

    // Used internally.  Does not notify the document of the change.  Only intended
    // for use on initial parse.
    void setDeclaration(PassRefPtr<CSSVariablesDeclaration> decl) { m_variables = decl; }

private:
    CSSVariablesRule(CSSStyleSheet* parent, MediaList*, bool variablesKeyword);

    RefPtr<MediaList> m_lstMedia;
    RefPtr<CSSVariablesDeclaration> m_variables;
    bool m_variablesKeyword;
};

} // namespace WebCore

#endif // CSSVariablesRule_h
