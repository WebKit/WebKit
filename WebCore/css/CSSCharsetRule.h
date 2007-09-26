/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef CSSCharsetRule_h
#define CSSCharsetRule_h

#include "CSSRule.h"
#include "PlatformString.h"

namespace WebCore {

typedef int ExceptionCode;

class CSSCharsetRule : public CSSRule {
public:
    CSSCharsetRule(StyleBase* parent, const String& encoding);
    virtual ~CSSCharsetRule();

    virtual bool isCharsetRule() { return true; }

    String encoding() const { return m_encoding; }
    void setEncoding(const String& encoding, ExceptionCode&) { m_encoding = encoding; }

    // Inherited from CSSRule
    virtual unsigned short type() const { return CHARSET_RULE; }

    virtual String cssText() const;

protected:
    String m_encoding;
};

} // namespace WebCore

#endif // CSSCharsetRule_h
