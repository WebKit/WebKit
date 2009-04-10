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

#ifndef CSSVariablesDeclaration_h
#define CSSVariablesDeclaration_h

#include "PlatformString.h"
#include "StringHash.h"
#include "StyleBase.h"
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

typedef int ExceptionCode;

class CSSMutableStyleDeclaration;
class CSSRule;
class CSSValue;
class CSSValueList;

class CSSVariablesDeclaration : public StyleBase {
public:
    static PassRefPtr<CSSVariablesDeclaration> create(StyleBase* owningRule, const Vector<String>& names, const Vector<RefPtr<CSSValue> >& values)
    {
        return adoptRef(new CSSVariablesDeclaration(owningRule, names, values));
    }
    virtual ~CSSVariablesDeclaration();

    String getVariableValue(const String&);
    String removeVariable(const String&, ExceptionCode&);
    void setVariable(const String&, const String&, ExceptionCode&);

    unsigned length() const;
    String item(unsigned index);

    CSSRule* parentRule();

    String cssText() const;
    void setCssText(const String&); // FIXME: The spec contradicts itself regarding whether or not cssText is settable.

    void addParsedVariable(const String& variableName, PassRefPtr<CSSValue> variableValue, bool updateNamesList = true);
    
    CSSValueList* getParsedVariable(const String& variableName);
    CSSMutableStyleDeclaration* getParsedVariableDeclarationBlock(const String& variableName);

private:
    CSSVariablesDeclaration(StyleBase* owningRule, const Vector<String>& names, const Vector<RefPtr<CSSValue> >& values);

    void setNeedsStyleRecalc();

protected:
    Vector<String> m_variableNames;
    HashMap<String, RefPtr<CSSValue> > m_variablesMap;
};

} // namespace WebCore

#endif // CSSVariablesDeclaration_h
