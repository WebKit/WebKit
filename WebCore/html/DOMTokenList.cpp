/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"
#include "DOMTokenList.h"

#include "Element.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "SpaceSplitString.h"
#include "StringBuilder.h"

namespace WebCore {

using namespace HTMLNames;

static bool validateToken(const AtomicString& token, ExceptionCode& ec)
{
    if (token.isEmpty()) {
        ec = SYNTAX_ERR;
        return false;
    }

    unsigned length = token.length();
    for (unsigned i = 0; i < length; ++i) {
        if (isHTMLSpace(token[i])) {
            ec = INVALID_CHARACTER_ERR;
            return false;
        }
    }

    return true;
}

DOMTokenList::DOMTokenList(Element* element)
    : m_element(element)
{
    if (m_element->document()->inQuirksMode())
        m_classNamesForQuirksMode.set(m_element->fastGetAttribute(classAttr), false);
}

void DOMTokenList::ref()
{
    m_element->ref();
}

void DOMTokenList::deref()
{
    m_element->deref();
}

unsigned DOMTokenList::length() const
{
    return classNames().size();
}

const AtomicString DOMTokenList::item(unsigned index) const
{
    if (index >= length())
        return AtomicString();
    return classNames()[index];
}

bool DOMTokenList::contains(const AtomicString& token, ExceptionCode& ec) const
{
    if (!validateToken(token, ec))
        return false;
    return containsInternal(token);
}

bool DOMTokenList::containsInternal(const AtomicString& token) const
{
    return classNames().contains(token);
}

void DOMTokenList::add(const AtomicString& token, ExceptionCode& ec)
{
    if (!validateToken(token, ec))
        return;
    addInternal(token);
}

void DOMTokenList::addInternal(const AtomicString& token) const
{
    const AtomicString& oldClassName(m_element->fastGetAttribute(classAttr));
    if (oldClassName.isEmpty())
        m_element->setAttribute(classAttr, token);
    else if (!containsInternal(token)) {
        StringBuilder builder;
        builder.append(oldClassName);
        if (oldClassName[oldClassName.length() - 1] != ' ')
            builder.append(' ');
        builder.append(token);
        m_element->setAttribute(classAttr, builder.toString());
    }
}

void DOMTokenList::remove(const AtomicString& token, ExceptionCode& ec)
{
    if (!validateToken(token, ec))
        return;
    removeInternal(token);
}

void DOMTokenList::removeInternal(const AtomicString& token) const
{
    // Check using contains first since it uses AtomicString comparisons instead
    // of character by character testing.
    if (!containsInternal(token))
        return;

    // Algorithm defined at http://www.whatwg.org/specs/web-apps/current-work/multipage/common-microsyntaxes.html#remove-a-token-from-a-string

    const AtomicString& input = m_element->fastGetAttribute(classAttr);
    unsigned inputLength = input.length();
    Vector<UChar> output; // 3
    output.reserveCapacity(inputLength);
    unsigned position = 0; // 4

    // Step 5
    while (position < inputLength) {
        if (isHTMLSpace(input[position])) { // 6
            output.append(input[position++]); // 6.1, 6.2
            continue; // 6.3
        }

        // Step 7
        Vector<UChar> s;
        while (position < inputLength && isNotHTMLSpace(input[position]))
            s.append(input[position++]);

        // Step 8
        if (s == token) {
            // Step 8.1
            while (position < inputLength && isHTMLSpace(input[position]))
                ++position;

            // Step 8.2
            size_t j = output.size();
            while (j > 0 && isHTMLSpace(output[j - 1]))
                --j;
            output.resize(j);

            // Step 8.3
            if (position < inputLength && !output.isEmpty())
                output.append(' ');
        } else
            output.append(s); // Step 9
    }

    output.shrinkToFit();
    m_element->setAttribute(classAttr, String::adopt(output));
}

bool DOMTokenList::toggle(const AtomicString& token, ExceptionCode& ec)
{
    if (!validateToken(token, ec))
        return false;

    if (containsInternal(token)) {
        removeInternal(token);
        return false;
    }
    addInternal(token);
    return true;
}

String DOMTokenList::toString() const
{
    return m_element->fastGetAttribute(classAttr);
}

void DOMTokenList::reset(const String& newClassName)
{
    if (!m_classNamesForQuirksMode.isNull())
        m_classNamesForQuirksMode.set(newClassName, false);
}

const SpaceSplitString& DOMTokenList::classNames() const
{
    if (!m_classNamesForQuirksMode.isNull())
        return m_classNamesForQuirksMode;
    return m_element->attributeMap()->classNames();
}

} // namespace WebCore
