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
#include "NodeInlines.h"
#include "RenderStyleInlines.h"

namespace WebCore {

AccessibilityMathMLElement::AccessibilityMathMLElement(AXID axID, RenderObject& renderer, bool isAnonymousOperator)
    : AccessibilityRenderObject(axID, renderer)
    , m_isAnonymousOperator(isAnonymousOperator)
{
}

AccessibilityMathMLElement::~AccessibilityMathMLElement() = default;

Ref<AccessibilityMathMLElement> AccessibilityMathMLElement::create(AXID axID, RenderObject& renderer, bool isAnonymousOperator)
{
    return adoptRef(*new AccessibilityMathMLElement(axID, renderer, isAnonymousOperator));
}

AccessibilityRole AccessibilityMathMLElement::determineAccessibilityRole()
{
    if (!m_renderer)
        return AccessibilityRole::Unknown;

    if ((m_ariaRole = determineAriaRoleAttribute()) != AccessibilityRole::Unknown)
        return m_ariaRole;

    if (WebCore::elementName(m_renderer->protectedNode().get()) == ElementName::MathML_math)
        return AccessibilityRole::DocumentMath;

    // It's not clear which role a platform should choose for a math element.
    // Declaring a math element role should give flexibility to platforms to choose.
    return AccessibilityRole::MathElement;
}

void AccessibilityMathMLElement::addChildren()
{
    if (!hasElementName(ElementName::MathML_mfenced)) {
        AccessibilityRenderObject::addChildren();
        return;
    }

    // mfenced elements generate lots of anonymous renderers due to their `open`, `close`, and `separators` attributes.
    // Because of this, default to walking the render tree when adding their children (unlike most other object types for
    // which we walk the DOM). This may cause unexpected behavior for `display:contents` descendants of mfenced elements.
    // However, this element is very deprecated, and even the most simple usages of it do not render consistently across
    // browsers, so it's already unlikely to be used by web developers, even more so with `display:contents` mixed in.
    m_childrenInitialized = true;
    for (Ref object : AXChildIterator(*this))
        addChild(WTFMove(object));

    m_subtreeDirty = false;

#ifndef NDEBUG
    verifyChildrenIndexInParent();
#endif
}

String AccessibilityMathMLElement::textUnderElement(TextUnderElementMode mode) const
{
    if (m_isAnonymousOperator && !mode.isHidden()) {
        char16_t operatorChar = downcast<RenderMathMLOperator>(*m_renderer).textContent();
        return operatorChar ? String(span(operatorChar)) : String();
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
    return m_renderer && m_renderer->isRenderMathMLRow() && !isMathRoot() && !isMathUnderOver() && !isMathMultiscript() && !isMathFraction();
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

bool AccessibilityMathMLElement::isMathFenceOperator() const
{
    auto* mathMLOperator = dynamicDowncast<RenderMathMLOperator>(renderer());
    return mathMLOperator && mathMLOperator->hasOperatorFlag(MathMLOperatorDictionary::Fence);
}

bool AccessibilityMathMLElement::isMathSeparatorOperator() const
{
    auto* mathMLOperator = dynamicDowncast<RenderMathMLOperator>(renderer());
    return mathMLOperator && mathMLOperator->hasOperatorFlag(MathMLOperatorDictionary::Separator);
}

bool AccessibilityMathMLElement::isMathText() const
{
    auto elementName = this->elementName();
    return elementName == ElementName::MathML_mtext || elementName == ElementName::MathML_ms;
}

bool AccessibilityMathMLElement::isMathNumber() const
{
    return elementName() == ElementName::MathML_mn;
}

bool AccessibilityMathMLElement::isMathIdentifier() const
{
    return elementName() == ElementName::MathML_mi;
}

bool AccessibilityMathMLElement::isMathMultiscript() const
{
    return elementName() == ElementName::MathML_mmultiscripts;
}

bool AccessibilityMathMLElement::isMathTable() const
{
    return elementName() == ElementName::MathML_mtable;
}

bool AccessibilityMathMLElement::isMathTableRow() const
{
    auto elementName = this->elementName();
    return elementName == ElementName::MathML_mtr || elementName == ElementName::MathML_mlabeledtr;
}

bool AccessibilityMathMLElement::isMathTableCell() const
{
    return elementName() == ElementName::MathML_mtd;
}

bool AccessibilityMathMLElement::isMathScriptObject(AccessibilityMathScriptObjectType type) const
{
    RefPtr parent = parentObjectUnignored();
    if (!parent)
        return false;

    return type == AccessibilityMathScriptObjectType::Subscript ? this == parent->mathSubscriptObject() : this == parent->mathSuperscriptObject();
}

bool AccessibilityMathMLElement::isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType type) const
{
    RefPtr parent = parentObjectUnignored();
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

std::optional<AXCoreObject::AccessibilityChildrenVector> AccessibilityMathMLElement::mathRadicand() 
{
    if (!isMathRoot())
        return std::nullopt;

    const auto& children = this->unignoredChildren();
    if (!children.size())
        return std::nullopt;

    if (isMathSquareRoot())
        return children;
    return { { children[0] } };
}

AXCoreObject* AccessibilityMathMLElement::mathRootIndexObject()
{
    if (!isMathRoot() || isMathSquareRoot())
        return nullptr;

    const auto& children = this->unignoredChildren();
    if (children.size() < 2)
        return nullptr;

    return children[1].ptr();
}

AXCoreObject* AccessibilityMathMLElement::mathNumeratorObject()
{
    if (!isMathFraction())
        return nullptr;

    const auto& children = this->unignoredChildren();
    if (children.size() != 2)
        return nullptr;

    return children[0].ptr();
}

AXCoreObject* AccessibilityMathMLElement::mathDenominatorObject()
{
    if (!isMathFraction())
        return nullptr;

    const auto& children = this->unignoredChildren();
    if (children.size() != 2)
        return nullptr;

    return children[1].ptr();
}

AXCoreObject* AccessibilityMathMLElement::mathUnderObject()
{
    if (!isMathUnderOver() || !element())
        return nullptr;

    const auto& children = this->unignoredChildren();
    if (children.size() < 2)
        return nullptr;

    auto elementName = this->elementName();
    if (elementName == ElementName::MathML_munder || elementName == ElementName::MathML_munderover)
        return children[1].ptr();

    return nullptr;
}

AXCoreObject* AccessibilityMathMLElement::mathOverObject()
{
    if (!isMathUnderOver() || !node())
        return nullptr;

    const auto& children = unignoredChildren();
    auto elementName = this->elementName();
    if (children.size() >= 2 && elementName == ElementName::MathML_mover)
        return children[1].ptr();

    if (children.size() >= 3 && elementName == ElementName::MathML_munderover)
        return children[2].ptr();

    return nullptr;
}

AXCoreObject* AccessibilityMathMLElement::mathBaseObject()
{
    if (!isMathSubscriptSuperscript() && !isMathUnderOver() && !isMathMultiscript())
        return nullptr;

    const auto& children = unignoredChildren();
    // The base object in question is always the first child.
    if (children.size() > 0)
        return children[0].ptr();

    return nullptr;
}

AXCoreObject* AccessibilityMathMLElement::mathSubscriptObject()
{
    if (!isMathSubscriptSuperscript() || !node())
        return nullptr;

    const auto& children = unignoredChildren();
    if (children.size() < 2)
        return nullptr;

    auto elementName = this->elementName();
    if (elementName == ElementName::MathML_msub || elementName == ElementName::MathML_msubsup)
        return children[1].ptr();

    return nullptr;
}

AXCoreObject* AccessibilityMathMLElement::mathSuperscriptObject()
{
    if (!isMathSubscriptSuperscript() || !node())
        return nullptr;

    const auto& children = unignoredChildren();
    unsigned count = children.size();

    auto elementName = this->elementName();
    if (count >= 2 && elementName == ElementName::MathML_msup)
        return children[1].ptr();

    if (count >= 3 && elementName == ElementName::MathML_msubsup)
        return children[2].ptr();

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
    for (RefPtr child = node()->firstChild(); child; child = child->nextSibling()) {
        if (foundPrescript) {
            RefPtr axChild = axObjectCache()->getOrCreate(*child);
            if (axChild && axChild->isMathElement()) {
                if (!prescriptPair.first)
                    prescriptPair.first = axChild.get();
                else {
                    prescriptPair.second = axChild.get();
                    prescripts.append(prescriptPair);
                    prescriptPair.first = nullptr;
                    prescriptPair.second = nullptr;
                }
            }
        } else if (WebCore::elementName(*child) == ElementName::MathML_mprescripts)
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
    for (RefPtr child = node()->firstChild(); child; child = child->nextSibling()) {
        if (WebCore::elementName(*child) == ElementName::MathML_mprescripts)
            break;

        RefPtr axChild = axObjectCache()->getOrCreate(*child);
        if (axChild && axChild->isMathElement()) {
            if (!foundBaseElement)
                foundBaseElement = true;
            else if (!postscriptPair.first)
                postscriptPair.first = axChild.get();
            else {
                postscriptPair.second = axChild.get();
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
    auto* fraction = dynamicDowncast<RenderMathMLFraction>(renderer());
    if (!fraction)
        return -1;

    return fraction->relativeLineThickness();
}

} // namespace WebCore

#endif // ENABLE(MATHML)
