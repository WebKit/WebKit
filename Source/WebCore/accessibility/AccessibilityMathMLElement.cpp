/*
 * Copyright (C) 2016 Igalia, S.L.
 * All rights reserved.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MATHML)
#include "AccessibilityMathMLElement.h"

#include "AXObjectCache.h"
#include "MathMLNames.h"

namespace WebCore {

AccessibilityMathMLElement::AccessibilityMathMLElement(RenderObject* renderer, bool isAnonymousOperator)
    : AccessibilityRenderObject(renderer)
    , m_isAnonymousOperator(isAnonymousOperator)
{
}

AccessibilityMathMLElement::~AccessibilityMathMLElement() = default;

Ref<AccessibilityMathMLElement> AccessibilityMathMLElement::create(RenderObject* renderer, bool isAnonymousOperator)
{
    return adoptRef(*new AccessibilityMathMLElement(renderer, isAnonymousOperator));
}

AccessibilityRole AccessibilityMathMLElement::determineAccessibilityRole()
{
    if (!m_renderer)
        return AccessibilityRole::Unknown;

    if ((m_ariaRole = determineAriaRoleAttribute()) != AccessibilityRole::Unknown)
        return m_ariaRole;

    Node* node = m_renderer->node();
    if (node && node->hasTagName(MathMLNames::mathTag))
        return AccessibilityRole::DocumentMath;

    // It's not clear which role a platform should choose for a math element.
    // Declaring a math element role should give flexibility to platforms to choose.
    return AccessibilityRole::MathElement;
}

String AccessibilityMathMLElement::textUnderElement(AccessibilityTextUnderElementMode mode) const
{
    if (m_isAnonymousOperator) {
        UChar operatorChar = downcast<RenderMathMLOperator>(*m_renderer).textContent();
        return operatorChar ? String(&operatorChar, 1) : String();
    }

    return AccessibilityRenderObject::textUnderElement(mode);
}

String AccessibilityMathMLElement::stringValue() const
{
    if (m_isAnonymousOperator)
        return textUnderElement();

    return AccessibilityRenderObject::stringValue();
}

bool AccessibilityMathMLElement::isIgnoredElementWithinMathTree() const
{
    if (m_isAnonymousOperator)
        return false;

    // Only math elements that we explicitly recognize should be included
    // We don't want things like <mstyle> to appear in the tree.
    if (isMathFraction() || isMathFenced() || isMathSubscriptSuperscript() || isMathRow()
        || isMathUnderOver() || isMathRoot() || isMathText() || isMathNumber()
        || isMathOperator() || isMathFenceOperator() || isMathSeparatorOperator()
        || isMathIdentifier() || isMathTable() || isMathTableRow() || isMathTableCell() || isMathMultiscript())
        return false;

    return true;
}

bool AccessibilityMathMLElement::isMathFraction() const
{
    return m_renderer && m_renderer->isRenderMathMLFraction();
}

bool AccessibilityMathMLElement::isMathFenced() const
{
    return m_renderer && m_renderer->isRenderMathMLFenced();
}

bool AccessibilityMathMLElement::isMathSubscriptSuperscript() const
{
    return m_renderer && m_renderer->isRenderMathMLScripts() && !isMathMultiscript();
}

bool AccessibilityMathMLElement::isMathRow() const
{
    return m_renderer && m_renderer->isRenderMathMLRow() && !isMathRoot();
}

bool AccessibilityMathMLElement::isMathUnderOver() const
{
    return m_renderer && m_renderer->isRenderMathMLUnderOver();
}

bool AccessibilityMathMLElement::isMathSquareRoot() const
{
    return m_renderer && m_renderer->isRenderMathMLSquareRoot();
}

bool AccessibilityMathMLElement::isMathToken() const
{
    return m_renderer && m_renderer->isRenderMathMLToken();
}

bool AccessibilityMathMLElement::isMathRoot() const
{
    return m_renderer && m_renderer->isRenderMathMLRoot();
}

bool AccessibilityMathMLElement::isMathOperator() const
{
    return m_renderer && m_renderer->isRenderMathMLOperator();
}

bool AccessibilityMathMLElement::isAnonymousMathOperator() const
{
    return m_isAnonymousOperator;
}

bool AccessibilityMathMLElement::isMathFenceOperator() const
{
    if (!is<RenderMathMLOperator>(renderer()))
        return false;

    return downcast<RenderMathMLOperator>(*m_renderer).hasOperatorFlag(MathMLOperatorDictionary::Fence);
}

bool AccessibilityMathMLElement::isMathSeparatorOperator() const
{
    if (!is<RenderMathMLOperator>(renderer()))
        return false;

    return downcast<RenderMathMLOperator>(*m_renderer).hasOperatorFlag(MathMLOperatorDictionary::Separator);
}

bool AccessibilityMathMLElement::isMathText() const
{
    return node() && (node()->hasTagName(MathMLNames::mtextTag) || hasTagName(MathMLNames::msTag));
}

bool AccessibilityMathMLElement::isMathNumber() const
{
    return node() && node()->hasTagName(MathMLNames::mnTag);
}

bool AccessibilityMathMLElement::isMathIdentifier() const
{
    return node() && node()->hasTagName(MathMLNames::miTag);
}

bool AccessibilityMathMLElement::isMathMultiscript() const
{
    return node() && node()->hasTagName(MathMLNames::mmultiscriptsTag);
}

bool AccessibilityMathMLElement::isMathTable() const
{
    return node() && node()->hasTagName(MathMLNames::mtableTag);
}

bool AccessibilityMathMLElement::isMathTableRow() const
{
    return node() && (node()->hasTagName(MathMLNames::mtrTag) || hasTagName(MathMLNames::mlabeledtrTag));
}

bool AccessibilityMathMLElement::isMathTableCell() const
{
    return node() && node()->hasTagName(MathMLNames::mtdTag);
}

bool AccessibilityMathMLElement::isMathScriptObject(AccessibilityMathScriptObjectType type) const
{
    AccessibilityObject* parent = parentObjectUnignored();
    if (!parent)
        return false;

    return type == AccessibilityMathScriptObjectType::Subscript ? this == parent->mathSubscriptObject() : this == parent->mathSuperscriptObject();
}

bool AccessibilityMathMLElement::isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType type) const
{
    AccessibilityObject* parent = parentObjectUnignored();
    if (!parent || !parent->isMathMultiscript())
        return false;

    // The scripts in a MathML <mmultiscripts> element consist of one or more
    // subscript, superscript pairs. In order to determine if this object is
    // a scripted token, we need to examine each set of pairs to see if the
    // this token is present and in the position corresponding with the type.

    AccessibilityMathMultiscriptPairs pairs;
    if (type == AccessibilityMathMultiscriptObjectType::PreSubscript || type == AccessibilityMathMultiscriptObjectType::PreSuperscript)
        parent->mathPrescripts(pairs);
    else
        parent->mathPostscripts(pairs);

    for (const auto& pair : pairs) {
        if (this == pair.first)
            return (type == AccessibilityMathMultiscriptObjectType::PreSubscript || type == AccessibilityMathMultiscriptObjectType::PostSubscript);
        if (this == pair.second)
            return (type == AccessibilityMathMultiscriptObjectType::PreSuperscript || type == AccessibilityMathMultiscriptObjectType::PostSuperscript);
    }

    return false;
}

AccessibilityObject* AccessibilityMathMLElement::mathRadicandObject()
{
    if (!isMathRoot())
        return nullptr;

    // For MathSquareRoot, we actually return the first child of the base.
    // See also https://webkit.org/b/146452
    const auto& children = this->children();
    if (children.size() < 1)
        return nullptr;

    return children[0].get();
}

AccessibilityObject* AccessibilityMathMLElement::mathRootIndexObject()
{
    if (!isMathRoot() || isMathSquareRoot())
        return nullptr;

    const auto& children = this->children();
    if (children.size() < 2)
        return nullptr;

    return children[1].get();
}

AccessibilityObject* AccessibilityMathMLElement::mathNumeratorObject()
{
    if (!isMathFraction())
        return nullptr;

    const auto& children = this->children();
    if (children.size() != 2)
        return nullptr;

    return children[0].get();
}

AccessibilityObject* AccessibilityMathMLElement::mathDenominatorObject()
{
    if (!isMathFraction())
        return nullptr;

    const auto& children = this->children();
    if (children.size() != 2)
        return nullptr;

    return children[1].get();
}

AccessibilityObject* AccessibilityMathMLElement::mathUnderObject()
{
    if (!isMathUnderOver() || !node())
        return nullptr;

    const auto& children = this->children();
    if (children.size() < 2)
        return nullptr;

    if (node()->hasTagName(MathMLNames::munderTag) || node()->hasTagName(MathMLNames::munderoverTag))
        return children[1].get();

    return nullptr;
}

AccessibilityObject* AccessibilityMathMLElement::mathOverObject()
{
    if (!isMathUnderOver() || !node())
        return nullptr;

    const auto& children = this->children();
    if (children.size() < 2)
        return nullptr;

    if (node()->hasTagName(MathMLNames::moverTag))
        return children[1].get();
    if (node()->hasTagName(MathMLNames::munderoverTag))
        return children[2].get();

    return nullptr;
}

AccessibilityObject* AccessibilityMathMLElement::mathBaseObject()
{
    if (!isMathSubscriptSuperscript() && !isMathUnderOver() && !isMathMultiscript())
        return nullptr;

    const auto& children = this->children();
    // The base object in question is always the first child.
    if (children.size() > 0)
        return children[0].get();

    return nullptr;
}

AccessibilityObject* AccessibilityMathMLElement::mathSubscriptObject()
{
    if (!isMathSubscriptSuperscript() || !node())
        return nullptr;

    const auto& children = this->children();
    if (children.size() < 2)
        return nullptr;

    if (node()->hasTagName(MathMLNames::msubTag) || node()->hasTagName(MathMLNames::msubsupTag))
        return children[1].get();

    return nullptr;
}

AccessibilityObject* AccessibilityMathMLElement::mathSuperscriptObject()
{
    if (!isMathSubscriptSuperscript() || !node())
        return nullptr;

    const auto& children = this->children();
    unsigned count = children.size();

    if (count >= 2 && node()->hasTagName(MathMLNames::msupTag))
        return children[1].get();

    if (count >= 3 && node()->hasTagName(MathMLNames::msubsupTag))
        return children[2].get();

    return nullptr;
}

String AccessibilityMathMLElement::mathFencedOpenString() const
{
    if (!isMathFenced())
        return String();

    return getAttribute(MathMLNames::openAttr);
}

String AccessibilityMathMLElement::mathFencedCloseString() const
{
    if (!isMathFenced())
        return String();

    return getAttribute(MathMLNames::closeAttr);
}

void AccessibilityMathMLElement::mathPrescripts(AccessibilityMathMultiscriptPairs& prescripts)
{
    if (!isMathMultiscript() || !node())
        return;

    bool foundPrescript = false;
    std::pair<AccessibilityObject*, AccessibilityObject*> prescriptPair;
    for (Node* child = node()->firstChild(); child; child = child->nextSibling()) {
        if (foundPrescript) {
            AccessibilityObject* axChild = axObjectCache()->getOrCreate(child);
            if (axChild && axChild->isMathElement()) {
                if (!prescriptPair.first)
                    prescriptPair.first = axChild;
                else {
                    prescriptPair.second = axChild;
                    prescripts.append(prescriptPair);
                    prescriptPair.first = nullptr;
                    prescriptPair.second = nullptr;
                }
            }
        } else if (child->hasTagName(MathMLNames::mprescriptsTag))
            foundPrescript = true;
    }

    // Handle the odd number of pre scripts case.
    if (prescriptPair.first)
        prescripts.append(prescriptPair);
}

void AccessibilityMathMLElement::mathPostscripts(AccessibilityMathMultiscriptPairs& postscripts)
{
    if (!isMathMultiscript() || !node())
        return;

    // In Multiscripts, the post-script elements start after the first element (which is the base)
    // and continue until a <mprescripts> tag is found
    std::pair<AccessibilityObject*, AccessibilityObject*> postscriptPair;
    bool foundBaseElement = false;
    for (Node* child = node()->firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(MathMLNames::mprescriptsTag))
            break;

        AccessibilityObject* axChild = axObjectCache()->getOrCreate(child);
        if (axChild && axChild->isMathElement()) {
            if (!foundBaseElement)
                foundBaseElement = true;
            else if (!postscriptPair.first)
                postscriptPair.first = axChild;
            else {
                postscriptPair.second = axChild;
                postscripts.append(postscriptPair);
                postscriptPair.first = nullptr;
                postscriptPair.second = nullptr;
            }
        }
    }

    // Handle the odd number of post scripts case.
    if (postscriptPair.first)
        postscripts.append(postscriptPair);
}

int AccessibilityMathMLElement::mathLineThickness() const
{
    if (!is<RenderMathMLFraction>(renderer()))
        return -1;

    return downcast<RenderMathMLFraction>(*m_renderer).relativeLineThickness();
}

} // namespace WebCore

#endif // ENABLE(MATHML)
