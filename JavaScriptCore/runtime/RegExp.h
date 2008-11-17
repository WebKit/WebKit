/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef KJS_REGEXP_H
#define KJS_REGEXP_H

#include "UString.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>

struct JSRegExp;

namespace JSC {

    class JSGlobalData;

    class RegExp : public RefCounted<RegExp> {
    public:
        static PassRefPtr<RegExp> create(JSGlobalData*, const UString& pattern);
        static PassRefPtr<RegExp> create(JSGlobalData*, const UString& pattern, const UString& flags);
        ~RegExp();

        bool global() const { return m_flagBits & Global; }
        bool ignoreCase() const { return m_flagBits & IgnoreCase; }
        bool multiline() const { return m_flagBits & Multiline; }

        const UString& pattern() const { return m_pattern; }
        const UString& flags() const { return m_flags; }

        bool isValid() const { return !m_constructionError; }
        const char* errorMessage() const { return m_constructionError; }

        int match(const UString&, int offset, OwnArrayPtr<int>* ovector = 0);
        unsigned numSubpatterns() const { return m_numSubpatterns; }

    private:
        RegExp(JSGlobalData*, const UString& pattern);
        RegExp(JSGlobalData*, const UString& pattern, const UString& flags);

        void compile();

        enum FlagBits { Global = 1, IgnoreCase = 2, Multiline = 4 };

        UString m_pattern; // FIXME: Just decompile m_regExp instead of storing this.
        UString m_flags; // FIXME: Just decompile m_regExp instead of storing this.
        int m_flagBits;
        JSRegExp* m_regExp;
        const char* m_constructionError;
        unsigned m_numSubpatterns;

#if ENABLE(WREC)
        // Called as a WRECFunction
        void* m_wrecFunction;
#endif
    };

} // namespace JSC

#endif // KJS_REGEXP_H
