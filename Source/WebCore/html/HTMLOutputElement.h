/*
 * Copyright (c) 2010 Google Inc. All rights reserved.
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

#pragma once

#include "DOMTokenList.h"
#include "HTMLFormControlElement.h"

namespace WebCore {

class HTMLOutputElement final : public HTMLFormControlElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLOutputElement);
public:
    static Ref<HTMLOutputElement> create(const QualifiedName&, Document&, HTMLFormElement*);

    String value() const;
    void setValue(const String&);
    String defaultValue() const;
    void setDefaultValue(const String&);
    DOMTokenList& htmlFor();
    
    bool canContainRangeEndPoint() const final { return false; }

private:
    HTMLOutputElement(const QualifiedName&, Document&, HTMLFormElement*);

    bool computeWillValidate() const final { return false; }
    void parseAttribute(const QualifiedName&, const AtomString&) final;
    const AtomString& formControlType() const final;
    bool isEnumeratable() const final { return true; }
    bool supportLabels() const final { return true; }
    bool supportsFocus() const final;
    void childrenChanged(const ChildChange&) final;
    void reset() final;

    void setTextContentInternal(const String&);

    bool m_isDefaultValueMode;
    bool m_isSetTextContentInProgress;
    String m_defaultValue;
    std::unique_ptr<DOMTokenList> m_tokens;
};

} // namespace WebCore
