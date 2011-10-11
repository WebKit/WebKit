/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef MutationObserverOptions_h
#define MutationObserverOptions_h

#if ENABLE(MUTATION_OBSERVERS)

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class MutationObserverOptions : public RefCounted<MutationObserverOptions> {
public:
    static PassRefPtr<MutationObserverOptions> create()
    {
        return adoptRef(new MutationObserverOptions);
    }

    bool childList() const { return m_childList; }
    void setChildList(bool value) { m_childList = value; }

    bool attributes() const { return m_attributes; }
    void setAttributes(bool value) { m_attributes = value; }

    bool characterData() const { return m_characterData; }
    void setCharacterData(bool value) { m_characterData = value; }

    bool subtree() const { return m_subtree; }
    void setSubtree(bool value) { m_subtree = value; }

    bool attributeOldValue() const { return m_attributeOldValue; }
    void setAttributeOldValue(bool value) { m_attributeOldValue = value; }

    bool characterDataOldValue() const { return m_attributeOldValue; }
    void setCharacterDataOldValue(bool value) { m_characterDataOldValue = value; }

    // FIXME: Figure out how to model this based on the IDL.
    // DOMString[] attributeFilter;

private:
    MutationObserverOptions()
        : m_childList(false)
        , m_attributes(false)
        , m_characterData(false)
        , m_subtree(false)
        , m_attributeOldValue(false)
        , m_characterDataOldValue(false)
    {
    }

    bool m_childList;
    bool m_attributes;
    bool m_characterData;
    bool m_subtree;
    bool m_attributeOldValue;
    bool m_characterDataOldValue;
};

} // namespace WebCore

#endif // ENABLE(MUTATION_OBSERVERS)

#endif // MutationObserverOptions_h
