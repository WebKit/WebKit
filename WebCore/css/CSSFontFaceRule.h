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

#ifndef CSSFontFaceRule_h
#define CSSFontFaceRule_h

#include "CSSRule.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSMutableStyleDeclaration;

class CSSFontFaceRule : public CSSRule {
public:
    CSSFontFaceRule(StyleBase* parent);
    virtual ~CSSFontFaceRule();

    virtual bool isFontFaceRule() { return true; }

    CSSMutableStyleDeclaration* style() const { return m_style.get(); }

    // Inherited from CSSRule
    virtual unsigned short type() const { return FONT_FACE_RULE; }

    virtual String cssText() const;

    void setDeclaration(PassRefPtr<CSSMutableStyleDeclaration>);

protected:
    RefPtr<CSSMutableStyleDeclaration> m_style;
};

} // namespace WebCore

#endif // CSSFontFaceRule_h
