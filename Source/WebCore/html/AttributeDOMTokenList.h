/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef AttributeDOMTokenList_h
#define AttributeDOMTokenList_h

#include "DOMTokenList.h"
#include "Element.h"

namespace WebCore {

class AttributeDOMTokenList final : public DOMTokenList {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AttributeDOMTokenList(Element&, const QualifiedName& attributeName);
    void attributeValueChanged(const AtomicString&);

private:
    virtual void ref() override { m_element.ref(); }
    virtual void deref() override { m_element.deref(); }

    virtual Element* element() const override { return &m_element; }
    virtual void updateAfterTokenChange() override;

    Element& m_element;
    const WebCore::QualifiedName& m_attributeName;
    bool m_isUpdatingAttributeValue { false };
};

} // namespace WebCore

#endif // AttributeDOMTokenList_h
