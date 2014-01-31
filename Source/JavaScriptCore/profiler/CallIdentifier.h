/*
 * Copyright (C) 2008, 2014 Apple Inc. All Rights Reserved.
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


#ifndef CallIdentifier_h
#define CallIdentifier_h

#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace JSC {

    struct CallIdentifier {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        CallIdentifier()
            : m_lineNumber(0)
            , m_columnNumber(0)
        {
        }

        CallIdentifier(const String& functionName, const String& url, unsigned lineNumber, unsigned columnNumber)
            : m_functionName(functionName)
            , m_url(!url.isNull() ? url : "")
            , m_lineNumber(lineNumber)
            , m_columnNumber(columnNumber)
        {
        }

        const String& functionName() const { return m_functionName; }

        const String& url() const { return m_url; }
        unsigned lineNumber() const { return m_lineNumber; }
        unsigned columnNumber() const { return m_columnNumber; }

        inline bool operator==(const CallIdentifier& other) const { return other.m_lineNumber == m_lineNumber && other.m_columnNumber == m_columnNumber && other.m_functionName == m_functionName && other.m_url == m_url; }
        inline bool operator!=(const CallIdentifier& other) const { return !(*this == other); }

        struct Hash {
            static unsigned hash(const CallIdentifier& key)
            {
                unsigned hashCodes[4] = {
                    key.m_functionName.impl()->hash(),
                    key.m_url.impl()->hash(),
                    key.m_lineNumber,
                    key.m_columnNumber
                };
                return StringHasher::hashMemory<sizeof(hashCodes)>(hashCodes);
            }

            static bool equal(const CallIdentifier& a, const CallIdentifier& b) { return a == b; }
            static const bool safeToCompareToEmptyOrDeleted = true;
        };

        unsigned hash() const { return Hash::hash(*this); }

#ifndef NDEBUG
        operator const char*() const { return c_str(); }
        const char* c_str() const { return m_name.utf8().data(); }
#endif

    private:
        String m_functionName;
        String m_url;
        unsigned m_lineNumber;
        unsigned m_columnNumber;
    };

} // namespace JSC

namespace WTF {

    template<> struct DefaultHash<JSC::CallIdentifier> { typedef JSC::CallIdentifier::Hash Hash; };

    template<> struct HashTraits<JSC::CallIdentifier> : GenericHashTraits<JSC::CallIdentifier> {
        static void constructDeletedValue(JSC::CallIdentifier& slot)
        {
            new (NotNull, &slot) JSC::CallIdentifier(String(), String(), std::numeric_limits<unsigned>::max(), std::numeric_limits<unsigned>::max());
        }

        static bool isDeletedValue(const JSC::CallIdentifier& value)
        {
            return value.functionName().isNull() && value.url().isNull() && value.lineNumber() == std::numeric_limits<unsigned>::max() && value.columnNumber() == std::numeric_limits<unsigned>::max();
        }
    };

} // namespace WTF

#endif  // CallIdentifier_h
