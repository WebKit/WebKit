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
 * Copyright (C) 2012, Google, Inc. All rights reserved.
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

#ifndef SiblingTraversalStrategies_h
#define SiblingTraversalStrategies_h

#include "Element.h"
#include "RenderStyle.h"

#include <wtf/UnusedParam.h>

namespace WebCore {

struct DOMSiblingTraversalStrategy {
    bool isFirstChild(Element*) const;
    bool isLastChild(Element*) const;
    bool isFirstOfType(Element*, const QualifiedName&) const;
    bool isLastOfType(Element*, const QualifiedName&) const;

    int countElementsBefore(Element*) const;
    int countElementsAfter(Element*) const;
    int countElementsOfTypeBefore(Element*, const QualifiedName&) const;
    int countElementsOfTypeAfter(Element*, const QualifiedName&) const;
};

inline bool DOMSiblingTraversalStrategy::isFirstChild(Element* element) const
{
    return !element->previousElementSibling();
}

inline bool DOMSiblingTraversalStrategy::isLastChild(Element* element) const
{
    return !element->nextElementSibling();
}

inline bool DOMSiblingTraversalStrategy::isFirstOfType(Element* element, const QualifiedName& type) const
{
    for (const Element* sibling = element->previousElementSibling(); sibling; sibling = sibling->previousElementSibling()) {
        if (sibling->hasTagName(type))
            return false;
    }
    return true;
}

inline bool DOMSiblingTraversalStrategy::isLastOfType(Element* element, const QualifiedName& type) const
{
    for (const Element* sibling = element->nextElementSibling(); sibling; sibling = sibling->nextElementSibling()) {
        if (sibling->hasTagName(type))
            return false;
    }
    return true;
}

inline int DOMSiblingTraversalStrategy::countElementsBefore(Element* element) const
{
    int count = 0;
    for (const Element* sibling = element->previousElementSibling(); sibling; sibling = sibling->previousElementSibling()) {
        RenderStyle* s = sibling->renderStyle();
        unsigned index = s ? s->childIndex() : 0;
        if (index) {
            count += index;
            break;
        }
        count++;
    }

    return count;
}

inline int DOMSiblingTraversalStrategy::countElementsOfTypeBefore(Element* element, const QualifiedName& type) const
{
    int count = 0;
    for (const Element* sibling = element->previousElementSibling(); sibling; sibling = sibling->previousElementSibling()) {
        if (sibling->hasTagName(type))
            ++count;
    }

    return count;
}

inline int DOMSiblingTraversalStrategy::countElementsAfter(Element* element) const
{
    int count = 0;
    for (const Element* sibling = element->nextElementSibling(); sibling; sibling = sibling->nextElementSibling())
        ++count;

    return count;
}

inline int DOMSiblingTraversalStrategy::countElementsOfTypeAfter(Element* element, const QualifiedName& type) const
{
    int count = 0;
    for (const Element* sibling = element->nextElementSibling(); sibling; sibling = sibling->nextElementSibling()) {
        if (sibling->hasTagName(type))
            ++count;
    }

    return count;
}

struct ShadowDOMSiblingTraversalStrategy {
    ShadowDOMSiblingTraversalStrategy(const Vector<RefPtr<Node> >& siblings, int nth)
        : m_siblings(siblings)
        , m_nth(nth)
    {
    }

    bool isFirstChild(Element*) const;
    bool isLastChild(Element*) const;
    bool isFirstOfType(Element*, const QualifiedName&) const;
    bool isLastOfType(Element*, const QualifiedName&) const;

    int countElementsBefore(Element*) const;
    int countElementsAfter(Element*) const;
    int countElementsOfTypeBefore(Element*, const QualifiedName&) const;
    int countElementsOfTypeAfter(Element*, const QualifiedName&) const;

private:
    Vector<RefPtr<Node> > m_siblings;
    int m_nth;
};

inline bool ShadowDOMSiblingTraversalStrategy::isFirstChild(Element* element) const
{
    UNUSED_PARAM(element);
    
    const Vector<RefPtr<Node> >& siblings = m_siblings;
    ASSERT(element == toElement(siblings[m_nth].get()));

    for (int i = m_nth - 1; i >= 0; --i) {
        if (siblings[i] && siblings[i]->isElementNode())
            return false;
    }

    return true;
}

inline bool ShadowDOMSiblingTraversalStrategy::isLastChild(Element* element) const
{
    UNUSED_PARAM(element);

    const Vector<RefPtr<Node> >& siblings = m_siblings;
    ASSERT(element == toElement(siblings[m_nth].get()));

    for (size_t i = m_nth + 1; i < siblings.size(); ++i) {
        if (siblings[i] && siblings[i]->isElementNode())
            return false;
    }

    return true;
}

inline bool ShadowDOMSiblingTraversalStrategy::isFirstOfType(Element* element, const QualifiedName& type) const
{
    UNUSED_PARAM(element);

    const Vector<RefPtr<Node> >& siblings = m_siblings;
    ASSERT(element == toElement(siblings[m_nth].get()));

    for (int i = m_nth - 1; i >= 0; --i) {
        if (siblings[i] && siblings[i]->isElementNode() && siblings[i]->hasTagName(type))
            return false;
    }

    return true;
}

inline bool ShadowDOMSiblingTraversalStrategy::isLastOfType(Element* element, const QualifiedName& type) const
{
    UNUSED_PARAM(element);

    const Vector<RefPtr<Node> >& siblings = m_siblings;
    ASSERT(element == toElement(siblings[m_nth].get()));

    for (size_t i = m_nth + 1; i < siblings.size(); ++i) {
        if (siblings[i] && siblings[i]->isElementNode() && siblings[i]->hasTagName(type))
            return false;
    }

    return true;
}

inline int ShadowDOMSiblingTraversalStrategy::countElementsBefore(Element* element) const
{
    UNUSED_PARAM(element);

    const Vector<RefPtr<Node> >& siblings = m_siblings;
    ASSERT(element == toElement(siblings[m_nth].get()));

    int count = 0;
    for (int i = m_nth - 1; i >= 0; --i) {
        if (siblings[i] && siblings[i]->isElementNode())
            ++count;
    }

    return count;
}

inline int ShadowDOMSiblingTraversalStrategy::countElementsAfter(Element* element) const
{
    UNUSED_PARAM(element);

    const Vector<RefPtr<Node> >& siblings = m_siblings;
    ASSERT(element == toElement(siblings[m_nth].get()));

    int count = 0;
    for (size_t i = m_nth + 1; i < siblings.size(); ++i) {
        if (siblings[i] && siblings[i]->isElementNode())
            return ++count;
    }

    return count;
}

inline int ShadowDOMSiblingTraversalStrategy::countElementsOfTypeBefore(Element* element, const QualifiedName& type) const
{
    UNUSED_PARAM(element);

    const Vector<RefPtr<Node> >& siblings = m_siblings;
    ASSERT(element == toElement(siblings[m_nth].get()));

    int count = 0;
    for (int i = m_nth - 1; i >= 0; --i) {
        if (siblings[i] && siblings[i]->isElementNode() && siblings[i]->hasTagName(type))
            ++count;
    }

    return count;
}

inline int ShadowDOMSiblingTraversalStrategy::countElementsOfTypeAfter(Element* element, const QualifiedName& type) const
{
    UNUSED_PARAM(element);

    const Vector<RefPtr<Node> >& siblings = m_siblings;
    ASSERT(element == toElement(siblings[m_nth].get()));

    int count = 0;
    for (size_t i = m_nth + 1; i < siblings.size(); ++i) {
        if (siblings[i] && siblings[i]->isElementNode() && siblings[i]->hasTagName(type))
            return ++count;
    }

    return count;
}

}

#endif
