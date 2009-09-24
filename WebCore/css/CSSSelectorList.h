/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef CSSSelectorList_h
#define CSSSelectorList_h

#include "CSSSelector.h"
#include <wtf/Noncopyable.h>

namespace WebCore {
    
class CSSSelectorList : public Noncopyable {
public:
    CSSSelectorList() : m_selectorArray(0) { }
    ~CSSSelectorList();

    void adopt(CSSSelectorList& list);
    void adoptSelectorVector(Vector<CSSSelector*>& selectorVector);
    
    CSSSelector* first() const { return m_selectorArray ? m_selectorArray : 0; }
    static CSSSelector* next(CSSSelector* previous) { return previous->isLastInSelectorList() ? 0 : previous + 1; }
    bool hasOneSelector() const { return m_selectorArray ? m_selectorArray->isLastInSelectorList() : false; }

    bool selectorsNeedNamespaceResolution();

private:
    void deleteSelectors();

    // End of the array is indicated by m_isLastInSelectorList bit in the last item.
    CSSSelector* m_selectorArray;
};

} // namespace WebCore

#endif // CSSSelectorList_h
