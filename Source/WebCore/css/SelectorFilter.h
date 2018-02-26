/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "Element.h"
#include <memory>
#include <wtf/BloomFilter.h>
#include <wtf/Vector.h>

namespace WebCore {

class CSSSelector;

class SelectorFilter {
public:
    void pushParent(Element* parent);
    void pushParentInitializingIfNeeded(Element& parent);
    void popParent();
    void popParentsUntil(Element* parent);
    bool parentStackIsEmpty() const { return m_parentStack.isEmpty(); }
    bool parentStackIsConsistent(const ContainerNode* parentNode) const;

    using Hashes = std::array<unsigned, 4>;
    bool fastRejectSelector(const Hashes&) const;
    static Hashes collectHashes(const CSSSelector&);

private:
    void initializeParentStack(Element& parent);

    struct ParentStackFrame {
        ParentStackFrame() : element(0) { }
        ParentStackFrame(Element* element) : element(element) { }
        Element* element;
        Vector<unsigned, 4> identifierHashes;
    };
    Vector<ParentStackFrame> m_parentStack;

    // With 100 unique strings in the filter, 2^12 slot table has false positive rate of ~0.2%.
    static const unsigned bloomFilterKeyBits = 12;
    CountingBloomFilter<bloomFilterKeyBits> m_ancestorIdentifierFilter;
};

inline bool SelectorFilter::fastRejectSelector(const Hashes& hashes) const
{
    for (auto& hash : hashes) {
        if (!hash)
            return false;
        if (!m_ancestorIdentifierFilter.mayContain(hash))
            return true;
    }
    return false;
}

} // namespace WebCore
