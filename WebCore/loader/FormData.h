/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef FormData_h
#define FormData_h

#include "DeprecatedValueList.h"
#include "PlatformString.h"
#include <wtf/Vector.h>

namespace WebCore {

class FormDataElement {
public:
    FormDataElement() : m_type(data) { }
    FormDataElement(const Vector<char>& array) : m_type(data), m_data(array) { }
    FormDataElement(const String& filename) : m_type(encodedFile), m_filename(filename) { }

    enum { data, encodedFile } m_type;
    Vector<char> m_data;
    String m_filename;
};

class FormData {
public:
    FormData() { } 
    FormData(const void* data, size_t);
    FormData(const CString&);

    void appendData(const void* data, size_t);
    void appendFile(const String& filename);

    void flatten(Vector<char>&) const; // omits files
    String flattenToString() const; // omits files

    const Vector<FormDataElement>& elements() const { return m_elements; }

private:
     Vector<FormDataElement> m_elements;
};

} // namespace WebCore

#endif
