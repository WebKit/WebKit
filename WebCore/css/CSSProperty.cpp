/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#include "config.h"
#include "CSSProperty.h"

#include "CSSPropertyNames.h"
#include "PlatformString.h"

namespace WebCore {

String CSSProperty::cssText() const
{
    if (id() == CSSPropertyWebkitVariableDeclarationBlock)
        return m_value->cssText() + ";";
    return String(getPropertyName(static_cast<CSSPropertyID>(id()))) + ": " + m_value->cssText() + (isImportant() ? " !important" : "") + "; ";
}

bool operator==(const CSSProperty& a, const CSSProperty& b)
{
    return a.m_id == b.m_id && a.m_important == b.m_important && a.m_value == b.m_value;
}

} // namespace WebCore
