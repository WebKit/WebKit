/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <WebCore/AXObjectCache.h>
#include <WebCore/Element.h>
#include <WebCore/Node.h>
#include <WebCore/RenderImage.h>

namespace WebCore {

enum class AXNotification : uint8_t;
enum class AccessibilityRole : uint8_t;
enum class NodeName : uint16_t;
class ContainerNode;
class Document;
class Element;
class Node;
class RenderImage;
class RenderStyle;
class RenderObject;
class StyleProperties;

bool hasRole(Element&, StringView role);
bool hasAnyRole(Element&, Vector<StringView>&& roles);
bool hasAnyRole(Element*, Vector<StringView>&& roles);
bool hasCellARIARole(Element&);
bool hasPresentationRole(Element&);
bool hasTableRole(Element&);
bool isRowGroup(Element&);
bool isRowGroup(Node*);
ContainerNode* composedParentIgnoringDocumentFragments(const Node&);
ContainerNode* composedParentIgnoringDocumentFragments(const Node*);

// Returns NodeName and not ElementName because it's impossible to forward declare ElementName.
NodeName elementName(Node*);
NodeName elementName(Node&);

RenderImage* toSimpleImage(RenderObject&);

// Returns true if the element has an attribute that will result in an accname being computed.
// https://www.w3.org/TR/accname-1.2/
bool hasAccNameAttribute(Element&);

bool isNodeFocused(Node&);

bool needsLayoutOrStyleRecalc(const Document&);

bool isRenderHidden(const RenderStyle*);
// Checks both CSS display properties, and CSS visibility properties.
bool isRenderHidden(const RenderStyle&);
// Only checks CSS visibility properties.
bool isVisibilityHidden(const RenderStyle&);
const RenderStyle* safeStyleFrom(Element&);

WTF::TextStream& operator<<(WTF::TextStream&, AXNotification);

void dumpAccessibilityTreeToStderr(Document&);

String roleToString(AccessibilityRole);

std::optional<CursorType> cursorTypeFrom(const StyleProperties&);

RefPtr<Node> lastNode(const Vector<AXID>&, AXObjectCache&);
RefPtr<AccessibilityObject> lastObject(const Vector<AXID>&, AXObjectCache&);

} // WebCore
