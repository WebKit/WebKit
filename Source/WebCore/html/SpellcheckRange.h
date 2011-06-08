/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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

#ifndef SpellcheckRange_h
#define SpellcheckRange_h

#if ENABLE(SPELLCHECK_API)

#include "DOMStringList.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class SpellcheckRange : public RefCounted<SpellcheckRange> {
public:
    static PassRefPtr<SpellcheckRange> create() { return adoptRef(new SpellcheckRange()); }
    static PassRefPtr<SpellcheckRange> create(unsigned long start, unsigned long length, PassRefPtr<DOMStringList> suggestions, unsigned short options)
    {
        return adoptRef(new SpellcheckRange(start, length, suggestions, options));
    }
    ~SpellcheckRange();

    // Implement the function defined in "SpellCheckRange.idl".
    unsigned long start() const { return m_start; }
    unsigned long length() const { return m_length; }
    DOMStringList* suggestions() const { return m_suggestions.get(); }
    unsigned short options() const { return m_options; }

private:
    SpellcheckRange();
    SpellcheckRange(unsigned long start, unsigned long length, PassRefPtr<DOMStringList> suggestions, unsigned short options);

    unsigned long m_start;
    unsigned long m_length;
    RefPtr<DOMStringList> m_suggestions;
    unsigned short m_options;
};

} // namespace WebCore

#endif

#endif // SpellcheckRange_h
