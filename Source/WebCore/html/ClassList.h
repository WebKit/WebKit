/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ClassList_h
#define ClassList_h

#include "DOMTokenList.h"
#include "SpaceSplitString.h"

namespace WebCore {

class Element;

class ClassList final : public DOMTokenList {
public:
    ClassList(Element& element)
        : m_element(element)
    {
    }

    virtual void ref() override;
    virtual void deref() override;

    virtual unsigned length() const override;
    virtual const AtomicString item(unsigned index) const override;

    virtual Element* element() const override;

    void clearValueForQuirksMode() { m_classNamesForQuirksMode.clear(); }

private:
    virtual bool containsInternal(const AtomicString&) const override;
    virtual AtomicString value() const override;
    virtual void setValue(const AtomicString&) override;

    const SpaceSplitString& classNames() const;

    Element& m_element;
    mutable SpaceSplitString m_classNamesForQuirksMode;
};

} // namespace WebCore

#endif // ClassList_h
