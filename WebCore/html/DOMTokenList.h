/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#ifndef DOMTokenList_h
#define DOMTokenList_h

#include "ExceptionCode.h"
#include "SpaceSplitString.h"
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class Element;

class DOMTokenList : public Noncopyable {
public:
    static PassOwnPtr<DOMTokenList> create(Element* element)
    {
        return adoptPtr(new DOMTokenList(element));
    }

    void ref();
    void deref();

    unsigned length() const;
    const AtomicString item(unsigned index) const;
    bool contains(const AtomicString&, ExceptionCode&) const;
    void add(const AtomicString&, ExceptionCode&);
    void remove(const AtomicString&, ExceptionCode&);
    bool toggle(const AtomicString&, ExceptionCode&);
    String toString() const;

    void reset(const String&);

    Element* element() { return m_element; }

private:
    DOMTokenList(Element*);

    void addInternal(const AtomicString&) const;
    bool containsInternal(const AtomicString&) const;
    void removeInternal(const AtomicString&) const;

    const SpaceSplitString& classNames() const;

    Element* m_element;
    SpaceSplitString m_classNamesForQuirksMode;
};

} // namespace WebCore

#endif // DOMTokenList_h
