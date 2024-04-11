/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
#include "AccessibilityARIAGridCell.h"

#include "AccessibilityObject.h"
#include "AccessibilityTable.h"
#include "HTMLNames.h"

namespace WebCore {
    
using namespace HTMLNames;

AccessibilityARIAGridCell::AccessibilityARIAGridCell(RenderObject& renderer)
    : AccessibilityTableCell(renderer)
{
}

AccessibilityARIAGridCell::AccessibilityARIAGridCell(Node& node)
    : AccessibilityTableCell(node)
{
}

AccessibilityARIAGridCell::~AccessibilityARIAGridCell() = default;

Ref<AccessibilityARIAGridCell> AccessibilityARIAGridCell::create(RenderObject& renderer)
{
    return adoptRef(*new AccessibilityARIAGridCell(renderer));
}

Ref<AccessibilityARIAGridCell> AccessibilityARIAGridCell::create(Node& node)
{
    return adoptRef(*new AccessibilityARIAGridCell(node));
}

AccessibilityTable* AccessibilityARIAGridCell::parentTable() const
{
    // ARIA gridcells may have multiple levels of unignored ancestors that are not the parent table,
    // including rows and interactive rowgroups. In addition, poorly-formed grids may contain elements
    // which pass the tests for inclusion.
    return dynamicDowncast<AccessibilityTable>(Accessibility::findAncestor<AccessibilityObject>(*this, false, [] (const auto& ancestor) {
        RefPtr ancestorTable = dynamicDowncast<AccessibilityTable>(ancestor);
        return ancestorTable && ancestorTable->isExposable() && !ancestorTable->accessibilityIsIgnored();
    }));
}
    
String AccessibilityARIAGridCell::readOnlyValue() const
{
    if (hasAttribute(aria_readonlyAttr))
        return getAttribute(aria_readonlyAttr).string().convertToASCIILowercase();

    // ARIA 1.1 requires user agents to propagate the grid's aria-readonly value to all
    // gridcell elements if the property is not present on the gridcell element itelf.
    if (RefPtr parent = parentTable())
        return parent->readOnlyValue();

    return String();
}
  
} // namespace WebCore
