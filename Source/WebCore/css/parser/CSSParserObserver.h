/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 - 2010  Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#pragma once

#include "StyleRule.h"

namespace WebCore {

class CSSParserToken;
class CSSParserTokenRange;

// This is only for the inspector and shouldn't be used elsewhere.
class CSSParserObserver {
public:
    virtual ~CSSParserObserver() { };
    virtual void startRuleHeader(StyleRuleType, unsigned offset) = 0;
    virtual void endRuleHeader(unsigned offset) = 0;
    virtual void observeSelector(unsigned startOffset, unsigned endOffset) = 0;
    virtual void startRuleBody(unsigned offset) = 0;
    virtual void endRuleBody(unsigned offset) = 0;
    virtual void observeProperty(unsigned startOffset, unsigned endOffset, bool isImportant, bool isParsed) = 0;
    virtual void observeComment(unsigned startOffset, unsigned endOffset) = 0;
};

} // namespace WebCore
