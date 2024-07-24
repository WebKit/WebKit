/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AXCoreObject.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXSearchManager);

enum class AccessibilitySearchKey {
    AnyType = 1,
    Article,
    BlockquoteSameLevel,
    Blockquote,
    BoldFont,
    Button,
    Checkbox,
    Control,
    DifferentType,
    FontChange,
    FontColorChange,
    Frame,
    Graphic,
    HeadingLevel1,
    HeadingLevel2,
    HeadingLevel3,
    HeadingLevel4,
    HeadingLevel5,
    HeadingLevel6,
    HeadingSameLevel,
    Heading,
    Highlighted,
    ItalicFont,
    KeyboardFocusable,
    Landmark,
    Link,
    List,
    LiveRegion,
    MisspelledWord,
    Outline,
    PlainText,
    RadioGroup,
    SameType,
    StaticText,
    StyleChange,
    TableSameLevel,
    Table,
    TextField,
    Underline,
    UnvisitedLink,
    VisitedLink,
};

struct AccessibilitySearchCriteria {
    // FIXME: change the object pointers to object IDs.
    AXCoreObject* anchorObject { nullptr };
    AXCoreObject* startObject { nullptr };
    CharacterRange startRange;
    AccessibilitySearchDirection searchDirection { AccessibilitySearchDirection::Next };
    Vector<AccessibilitySearchKey> searchKeys;
    String searchText;
    unsigned resultsLimit { 0 };
    bool visibleOnly { false };
    bool immediateDescendantsOnly { false };
};

class AXSearchManager {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(AXSearchManager);
public:
    AXCoreObject::AccessibilityChildrenVector findMatchingObjects(AccessibilitySearchCriteria&&);
    std::optional<AXTextMarkerRange> findMatchingRange(AccessibilitySearchCriteria&&);
private:
    AXCoreObject::AccessibilityChildrenVector findMatchingObjectsInternal(const AccessibilitySearchCriteria&);
    bool matchWithResultsLimit(RefPtr<AXCoreObject>, const AccessibilitySearchCriteria&, AXCoreObject::AccessibilityChildrenVector&);
    bool match(RefPtr<AXCoreObject>, const AccessibilitySearchCriteria&);
    bool matchText(RefPtr<AXCoreObject>, const String&);
    bool matchForSearchKeyAtIndex(RefPtr<AXCoreObject>, const AccessibilitySearchCriteria&, size_t);

    // Keeps the ranges of misspellings for each object.
    HashMap<AXID, Vector<CharacterRange>> m_spellCheckerResultRanges;
};

inline AXCoreObject::AccessibilityChildrenVector AXSearchManager::findMatchingObjects(AccessibilitySearchCriteria&& criteria)
{
    return findMatchingObjectsInternal(std::forward<AccessibilitySearchCriteria>(criteria));
}

WTF::TextStream& operator<<(WTF::TextStream&, AccessibilitySearchKey);
WTF::TextStream& operator<<(WTF::TextStream&, const AccessibilitySearchCriteria&);

} // namespace WebCore
