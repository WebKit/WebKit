/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2012 Apple Inc. All rights reserved.
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
#include "CSSRuleList.h"

#include "CSSRule.h"

namespace WebCore {

CSSRuleList::CSSRuleList() = default;

CSSRuleList::~CSSRuleList() = default;

StaticCSSRuleList::StaticCSSRuleList() = default;

StaticCSSRuleList::~StaticCSSRuleList() = default;

#ifndef RC_REMOVE_SEC_ACCEL_3
WebCoreOpaqueRoot StaticCSSRuleList::opaqueRootForGCThread() const
{
    // Unlike the live lists, a static list owns already-created wrappers, so there is nothing to
    // create lazily here. The list is filled on the main thread before it is ever handed to script,
    // and the wrappers it holds are parentless clones, so this only reads.
    if (m_rules.isEmpty())
        return WebCoreOpaqueRoot { nullptr };
    return m_rules[0]->opaqueRootForGCThread();
}
#endif // RC_REMOVE_SEC_ACCEL_3

} // namespace WebCore
