/*
 * Copyright (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "ClassNames.h"

namespace WebCore {

void ClassNames::parseClassAttribute(const String& classStr, const bool inCompatMode)
{
    if (!m_nameVector)
        m_nameVector.set(new ClassNameVector);
    else
        m_nameVector->clear();

    if (classStr.isEmpty())
        return;

    String classAttr = inCompatMode ? classStr.foldCase() : classStr;

    const UChar* str = classAttr.characters();
    const int length = classAttr.length();
    int start = 0;
    while (true) {
        while (start < length && isClassWhitespace(str[start]))
            ++start;
        if (start >= length)
            break;
        int end = start + 1;
        while (end < length && !isClassWhitespace(str[end]))
            ++end;

        m_nameVector->append(AtomicString(str + start, end - start));

        start = end + 1;
    }
}

} // namespace WebCore
