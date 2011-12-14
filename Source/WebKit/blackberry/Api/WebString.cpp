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

#include "config.h"
#include "WebString.h"

#include "WebStringImpl.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace BlackBerry {
namespace WebKit {

WebString::WebString(const char* latin1)
    : m_impl(static_cast<WebStringImpl*>(WTF::StringImpl::create(latin1).releaseRef()))
{
}

WebString::WebString(const char* latin1, unsigned length)
    : m_impl(static_cast<WebStringImpl*>(WTF::StringImpl::create(latin1, length).releaseRef()))
{
}

WebString::WebString(const unsigned short* utf16, unsigned length)
    : m_impl(static_cast<WebStringImpl*>(WTF::StringImpl::create(utf16, length).releaseRef()))
{
}

WebString::WebString(WebStringImpl* impl)
    : m_impl(impl)
{
    if (m_impl)
        m_impl->ref();
}

WebString::~WebString()
{
    if (m_impl)
        m_impl->deref();
}

WebString::WebString(const WebString& str)
    : m_impl(str.m_impl)
{
    if (m_impl)
        m_impl->ref();
}

WebString WebString::fromUtf8(const char* utf8)
{
    return String::fromUTF8(utf8);
}

WebString& WebString::operator=(const WebString& str)
{
    if (str.m_impl)
        str.m_impl->ref();
    if (m_impl)
        m_impl->deref();
    m_impl = str.m_impl;
    return *this;
}

std::string WebString::utf8() const
{
    std::string utf8;
    if (!m_impl)
        return utf8;

    CString cstr = String(m_impl).utf8();
    utf8.assign(cstr.data(), cstr.length());
    return utf8;
}

const unsigned short* WebString::characters() const
{
    return m_impl ? m_impl->characters() : 0;
}

unsigned WebString::length() const
{
    return m_impl ? m_impl->length() : 0;
}

bool WebString::isEmpty() const
{
    return !m_impl || !m_impl->length();
}

bool WebString::equal(const char* utf8) const
{
    return WTF::equal(m_impl, utf8);
}

bool WebString::equalIgnoringCase(const char* utf8) const
{
    return WTF::equalIgnoringCase(m_impl, utf8);
}

} // namespace WebKit
} // namespace BlackBerry
