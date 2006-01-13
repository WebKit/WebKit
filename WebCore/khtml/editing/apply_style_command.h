/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef __apply_style_command_h__
#define __apply_style_command_h__

#include "composite_edit_command.h"
#include <kxmlcore/PassRefPtr.h>

namespace DOM {
    class HTMLElementImpl;
}

namespace khtml {

class ApplyStyleCommand : public CompositeEditCommand
{
public:
    enum EPropertyLevel { PropertyDefault, ForceBlockProperties };

    ApplyStyleCommand(DOM::DocumentImpl *, DOM::CSSStyleDeclarationImpl *style, EditAction editingAction=EditActionChangeAttributes, EPropertyLevel=PropertyDefault);
    ApplyStyleCommand(DOM::DocumentImpl *, DOM::CSSStyleDeclarationImpl *style, const DOM::Position start, const DOM::Position end, EditAction editingAction=EditActionChangeAttributes, EPropertyLevel=PropertyDefault);
    virtual ~ApplyStyleCommand();

    virtual void doApply();
    virtual EditAction editingAction() const;

    DOM::CSSMutableStyleDeclarationImpl *style() const { return m_style.get(); }

private:
    // style-removal helpers
    bool isHTMLStyleNode(DOM::CSSMutableStyleDeclarationImpl *, DOM::HTMLElementImpl *);
    void removeHTMLStyleNode(DOM::HTMLElementImpl *);
    void removeHTMLFontStyle(DOM::CSSMutableStyleDeclarationImpl *, DOM::HTMLElementImpl *);
    void removeCSSStyle(DOM::CSSMutableStyleDeclarationImpl *, DOM::HTMLElementImpl *);
    void removeBlockStyle(DOM::CSSMutableStyleDeclarationImpl *, const DOM::Position &start, const DOM::Position &end);
    void removeInlineStyle(PassRefPtr<DOM::CSSMutableStyleDeclarationImpl>, const DOM::Position &start, const DOM::Position &end);
    bool nodeFullySelected(DOM::NodeImpl *, const DOM::Position &start, const DOM::Position &end) const;
    bool nodeFullyUnselected(DOM::NodeImpl *node, const DOM::Position &start, const DOM::Position &end) const;
    DOM::CSSMutableStyleDeclarationImpl *extractTextDecorationStyle(DOM::NodeImpl *node);
    DOM::CSSMutableStyleDeclarationImpl *extractAndNegateTextDecorationStyle(DOM::NodeImpl *node);
    void applyTextDecorationStyle(DOM::NodeImpl *node, DOM::CSSMutableStyleDeclarationImpl *style);
    void pushDownTextDecorationStyleAroundNode(DOM::NodeImpl *node, const DOM::Position &start, const DOM::Position &end, bool force);
    void pushDownTextDecorationStyleAtBoundaries(const DOM::Position &start, const DOM::Position &end);
    
    // style-application helpers
    void applyBlockStyle(DOM::CSSMutableStyleDeclarationImpl *);
    void applyRelativeFontStyleChange(DOM::CSSMutableStyleDeclarationImpl *);
    void applyInlineStyle(DOM::CSSMutableStyleDeclarationImpl *);
    void addBlockStyleIfNeeded(DOM::CSSMutableStyleDeclarationImpl *, DOM::NodeImpl *);
    void addInlineStyleIfNeeded(DOM::CSSMutableStyleDeclarationImpl *, DOM::NodeImpl *start, DOM::NodeImpl *end);
    bool splitTextAtStartIfNeeded(const DOM::Position &start, const DOM::Position &end);
    bool splitTextAtEndIfNeeded(const DOM::Position &start, const DOM::Position &end);
    bool splitTextElementAtStartIfNeeded(const DOM::Position &start, const DOM::Position &end);
    bool splitTextElementAtEndIfNeeded(const DOM::Position &start, const DOM::Position &end);
    bool mergeStartWithPreviousIfIdentical(const DOM::Position &start, const DOM::Position &end);
    bool mergeEndWithNextIfIdentical(const DOM::Position &start, const DOM::Position &end);
    void cleanUpEmptyStyleSpans(const DOM::Position &start, const DOM::Position &end);

    void surroundNodeRangeWithElement(DOM::NodeImpl *start, DOM::NodeImpl *end, DOM::ElementImpl *element);
    float computedFontSize(const DOM::NodeImpl *);
    void joinChildTextNodes(DOM::NodeImpl *, const DOM::Position &start, const DOM::Position &end);

    void ApplyStyleCommand::updateStartEnd(DOM::Position newStart, DOM::Position newEnd);
    DOM::Position ApplyStyleCommand::startPosition();
    DOM::Position ApplyStyleCommand::endPosition();
    
    DOM::Position m_start;
    DOM::Position m_end;
    RefPtr<DOM::CSSMutableStyleDeclarationImpl> m_style;
    EditAction m_editingAction;
    EPropertyLevel m_propertyLevel;
    bool m_useEndingSelection;
};

bool isStyleSpan(const DOM::NodeImpl *node);
DOM::ElementImpl *createStyleSpanElement(DOM::DocumentImpl *document);

} // namespace khtml

#endif // __apply_style_command_h__
