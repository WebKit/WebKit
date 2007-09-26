/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
#include "CSSRule.h"

#include "CSSStyleSheet.h"

namespace WebCore {

CSSStyleSheet* CSSRule::parentStyleSheet() const
{
    return (parent() && parent()->isCSSStyleSheet()) ? static_cast<CSSStyleSheet*>(parent()) : 0;
}

CSSRule* CSSRule::parentRule() const
{
    return (parent() && parent()->isRule()) ? static_cast<CSSRule*>(parent()) : 0;
}

String CSSRule::cssText() const
{
    // FIXME: Implement!
    return String();
}

void CSSRule::setCssText(String /*cssText*/, ExceptionCode& /*ec*/)
{
    // FIXME: Implement!
}

} // namespace WebCore
