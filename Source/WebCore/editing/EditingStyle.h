/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#ifndef EditingStyle_h
#define EditingStyle_h

#include "CSSPropertyNames.h"
#include "WritingDirection.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class CSSStyleDeclaration;
class CSSComputedStyleDeclaration;
class CSSMutableStyleDeclaration;
class Document;
class HTMLElement;
class Node;
class Position;
class QualifiedName;
class RenderStyle;
class StyledElement;

class EditingStyle : public RefCounted<EditingStyle> {
public:

    enum PropertiesToInclude { AllProperties, OnlyInheritableProperties };
    enum ShouldPreserveWritingDirection { PreserveWritingDirection, DoNotPreserveWritingDirection };
    enum ShouldExtractMatchingStyle { ExtractMatchingStyle, DoNotExtractMatchingStyle };
    static float NoFontDelta;

    static PassRefPtr<EditingStyle> create()
    {
        return adoptRef(new EditingStyle());
    }

    static PassRefPtr<EditingStyle> create(Node* node, PropertiesToInclude propertiesToInclude = OnlyInheritableProperties)
    {
        return adoptRef(new EditingStyle(node, propertiesToInclude));
    }

    static PassRefPtr<EditingStyle> create(const Position& position)
    {
        return adoptRef(new EditingStyle(position));
    }

    static PassRefPtr<EditingStyle> create(const CSSStyleDeclaration* style)
    {
        return adoptRef(new EditingStyle(style));
    }

    ~EditingStyle();

    CSSMutableStyleDeclaration* style() { return m_mutableStyle.get(); }
    bool textDirection(WritingDirection&) const;
    bool isEmpty() const;
    void setStyle(PassRefPtr<CSSMutableStyleDeclaration>);
    void overrideWithStyle(const CSSMutableStyleDeclaration*);
    void clear();
    PassRefPtr<EditingStyle> copy() const;
    PassRefPtr<EditingStyle> extractAndRemoveBlockProperties();
    PassRefPtr<EditingStyle> extractAndRemoveTextDirection();
    void removeBlockProperties();
    void removeStyleAddedByNode(Node*);
    void removeStyleConflictingWithStyleOfNode(Node*);
    void removeNonEditingProperties();
    void collapseTextDecorationProperties();
    bool conflictsWithInlineStyleOfElement(StyledElement* element) const { return conflictsWithInlineStyleOfElement(element, 0, 0); }
    bool conflictsWithInlineStyleOfElement(StyledElement* element, EditingStyle* extractedStyle, Vector<CSSPropertyID>& conflictingProperties) const
    {
        return conflictsWithInlineStyleOfElement(element, extractedStyle, &conflictingProperties);
    }
    bool conflictsWithImplicitStyleOfElement(HTMLElement*, EditingStyle* extractedStyle = 0, ShouldExtractMatchingStyle = DoNotExtractMatchingStyle) const;
    bool conflictsWithImplicitStyleOfAttributes(HTMLElement*) const;
    bool extractConflictingImplicitStyleOfAttributes(HTMLElement*, ShouldPreserveWritingDirection, EditingStyle* extractedStyle,
            Vector<QualifiedName>& conflictingAttributes, ShouldExtractMatchingStyle) const;
    void prepareToApplyAt(const Position&, ShouldPreserveWritingDirection = DoNotPreserveWritingDirection);
    void mergeTypingStyle(Document*);
    void mergeInlineStyleOfElement(StyledElement*);

    float fontSizeDelta() const { return m_fontSizeDelta; }
    bool hasFontSizeDelta() const { return m_fontSizeDelta != NoFontDelta; }
    bool shouldUseFixedDefaultFontSize() const { return m_shouldUseFixedDefaultFontSize; }

private:
    EditingStyle();
    EditingStyle(Node*, PropertiesToInclude);
    EditingStyle(const Position&);
    EditingStyle(const CSSStyleDeclaration*);
    void init(Node*, PropertiesToInclude);
    void removeTextFillAndStrokeColorsIfNeeded(RenderStyle*);
    void setProperty(int propertyID, const String& value, bool important = false);
    void replaceFontSizeByKeywordIfPossible(RenderStyle*, CSSComputedStyleDeclaration*);
    void extractFontSizeDelta();
    bool conflictsWithInlineStyleOfElement(StyledElement*, EditingStyle* extractedStyle, Vector<CSSPropertyID>* conflictingProperties) const;
    void mergeStyle(CSSMutableStyleDeclaration*);

    RefPtr<CSSMutableStyleDeclaration> m_mutableStyle;
    bool m_shouldUseFixedDefaultFontSize;
    float m_fontSizeDelta;

    friend class HTMLElementEquivalent;
    friend class HTMLAttributeEquivalent;
};

} // namespace WebCore

#endif // EditingStyle_h
