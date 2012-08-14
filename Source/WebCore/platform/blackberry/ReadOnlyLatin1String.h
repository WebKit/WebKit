/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef ReadOnlyLatin1String_h
#define ReadOnlyLatin1String_h

#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ReadOnlyLatin1String {
public:
    explicit ReadOnlyLatin1String(const String& string)
    {
        if (string.is8Bit())
            m_string = string;
        else {
            ASSERT(string.containsOnlyLatin1());
            m_cstring = string.latin1();
        }
    }

    const char* data() const { return m_string.isNull() ? m_cstring.data() : reinterpret_cast<const char*>(m_string.characters8()); }

    size_t length() const { return m_string.isNull() ? m_cstring.length() : m_string.length(); }

private:
    String m_string;
    CString m_cstring;
};

} // namespace WebCore

#endif
