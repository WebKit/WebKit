/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
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

#ifndef SmallStrings_h
#define SmallStrings_h

#include "UString.h"
#include <wtf/OwnPtr.h>

namespace JSC {

    class JSGlobalData;
    class JSString;
    class MarkStack;
    class SmallStringsStorage;

    class SmallStrings : public Noncopyable {
    public:
        SmallStrings();
        ~SmallStrings();

        JSString* emptyString(JSGlobalData* globalData)
        {
            if (!m_emptyString)
                createEmptyString(globalData);
            return m_emptyString;
        }
        JSString* singleCharacterString(JSGlobalData* globalData, unsigned char character)
        {
            if (!m_singleCharacterStrings[character])
                createSingleCharacterString(globalData, character);
            return m_singleCharacterStrings[character];
        }

        UString::Rep* singleCharacterStringRep(unsigned char character);

        void markChildren(MarkStack&);

        unsigned count() const;

    private:
        void createEmptyString(JSGlobalData*);
        void createSingleCharacterString(JSGlobalData*, unsigned char);

        JSString* m_emptyString;
        JSString* m_singleCharacterStrings[0x100];
        OwnPtr<SmallStringsStorage> m_storage;
    };

} // namespace JSC

#endif // SmallStrings_h
