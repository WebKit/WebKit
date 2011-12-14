/*
 * Copyright (C) 2009, 2010, 2011 Research In Motion Limited. All rights reserved.
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

#ifndef WebString_h
#define WebString_h

#include "BlackBerryGlobal.h"
#include <string>

namespace BlackBerry {
namespace WebKit {

class WebStringImpl;

class BLACKBERRY_EXPORT WebString {
public:
    WebString() : m_impl(0) { }
    ~WebString();
    WebString(const char* latin1);
    WebString(const char* latin1, unsigned length);
    WebString(const unsigned short* utf16, unsigned length);
    WebString(WebStringImpl*);
    WebString(const WebString&);
    WebString& operator=(const WebString&);
    std::string utf8() const;
    static WebString fromUtf8(const char* utf8);
    const unsigned short* characters() const;
    unsigned length() const;
    bool isEmpty() const;
    bool equal(const char* utf8) const;
    bool equalIgnoringCase(const char* utf8) const;
    WebStringImpl* impl() const { return m_impl; }
private:
    WebStringImpl* m_impl;
};
} // namespace WebKit
} // namespace BlackBerry

#endif // WebString_h
