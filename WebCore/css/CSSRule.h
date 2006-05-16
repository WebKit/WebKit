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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef CSSRule_H
#define CSSRule_H

#include "css_base.h" // for StyleBase

namespace WebCore {

class CSSStyleSheet;

class CSSRule : public StyleBase
{
public:
    enum CSSRuleType { 
        UNKNOWN_RULE, 
        STYLE_RULE, 
        CHARSET_RULE, 
        IMPORT_RULE, 
        MEDIA_RULE, 
        FONT_FACE_RULE, 
        PAGE_RULE 
    };
    
    CSSRule(StyleBase* parent) : StyleBase(parent), m_type(UNKNOWN_RULE) { }

    virtual bool isRule() { return true; }
    unsigned short type() const { return m_type; }

    CSSStyleSheet* parentStyleSheet() const;
    CSSRule* parentRule() const;

    virtual String cssText() const;
    void setCssText(String str);

protected:
    CSSRuleType m_type;
};

} // namespace

#endif
