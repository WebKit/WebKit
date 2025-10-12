/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/AccessibilityRole.h>

namespace WebCore {

class AXObjectCache;
class Node;
class Element;
class HTMLTableElement;
class HTMLTableSectionElement;
class RenderObject;
class RenderStyle;
struct AccessibilityText;

namespace AXTableHelpers {

bool appendCaptionTextIfNecessary(Element&, Vector<AccessibilityText>&);
bool isTableRole(AccessibilityRole);
bool isTableRowElement(Element&);
bool isTableCellElement(Element&);
bool hasRowRole(Element&);
HTMLTableElement* tableElementIncludingAncestors(Node*, RenderObject*);
bool tableElementIndicatesAccessibleTable(HTMLTableElement&);
bool tableSectionIndicatesAccessibleTable(HTMLTableSectionElement&, AXObjectCache&);
bool isDataTableWithTraversal(HTMLTableElement&, AXObjectCache&);

// This value is what will be used if table cells determine the cell
// should not be treated as a cell (e.g. because it is in a layout table).
static constexpr AccessibilityRole layoutTableCellRole = AccessibilityRole::TextGroup;

} // namespace AXTableHelpers

} // namespace WebCore
