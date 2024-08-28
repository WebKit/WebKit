/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AXTreeFilter.h"

#include "AXLogger.h"
#include "AccessibilityARIAGridRow.h"
#include "AccessibilityTable.h"
#include "AccessibilityTableCell.h"

namespace WebCore {

AXCoreObject* AXTreeFilter::parent(const AXCoreObject& object)
{
    if (auto* axObject = dynamicDowncast<AccessibilityObject>(object))
        return parent(*axObject);
    if (auto* isolatedObject = dynamicDowncast<AXIsolatedObject>(object))
        return parent(*isolatedObject);
    return nullptr;
}

bool AXTreeFilter::isIgnored(const AccessibilityObject& object)
{
    AXTRACE("AXTreeFilter::isIgnored"_s);

    if (object.isLabel()) {
        // FIXME: this is a "naive attempt" to eliminate duplication of labels. More work on this is needed.
        RefPtr nextSibling = object.nextSiblingUnignored(10);
        if (nextSibling && nextSibling->isTextControl()) {
            String label = object.title();
            String text = nextSibling->title();
            if (label == text)
                return true;
        }
    }
    return false;
}

AccessibilityObject* AXTreeFilter::parent(const AccessibilityObject& object)
{
    if (auto* cell = dynamicDowncast<AccessibilityTableCell>(&object)) {
        if (auto* row = cell->ariaOwnedByParent())
            return row;
    } else if (auto* gridRow = dynamicDowncast<AccessibilityARIAGridRow>(&object)) {
        if (auto* table = gridRow->parentTable())
            return table;
    }

    return Accessibility::findAncestor(object, false, [] (const auto& ancestor) {
        return !ancestor.isIgnored();
    });
}

AXIsolatedObject* AXTreeFilter::parent(const AXIsolatedObject& object)
{
    return object.parentObject();
}

} // namespace WebCore
