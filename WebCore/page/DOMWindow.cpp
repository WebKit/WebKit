/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#include "DOMWindow.h"

#include "cssstyleselector.h"
#include "CSSComputedStyleDeclaration.h"
#include "Document.h"
#include "Element.h"
#include "Frame.h"

namespace WebCore {

DOMWindow::DOMWindow(Frame* f)
    : m_frame(f)
{
}

Frame* DOMWindow::frame()
{
    return m_frame;
}

void DOMWindow::disconnectFrame()
{
    m_frame = 0;
}

Document* DOMWindow::document() const
{
    if (!m_frame)
        return 0;
    
    if (!m_frame->document()) {
        m_frame->createEmptyDocument();
        m_frame->begin();
        m_frame->write("<HTML><BODY>");
        m_frame->end();
    }
    return m_frame->document();
}

PassRefPtr<CSSStyleDeclaration> DOMWindow::getComputedStyle(Element* elt, const String&) const
{
    // FIXME: This should work even if we do not have a renderer.
    // FIXME: This needs to work with pseudo elements.
    if (!elt || !elt->renderer())
        return 0;
    
    return new CSSComputedStyleDeclaration(elt);
}

PassRefPtr<CSSRuleList> DOMWindow::getMatchedCSSRules(Element* elt, const String& pseudoElt, bool authorOnly) const
{
    if (!m_frame || !m_frame->document())
        return 0;
    
    if (!pseudoElt.isEmpty())
        return m_frame->document()->styleSelector()->pseudoStyleRulesForElement(elt, pseudoElt.impl(), authorOnly);
    return m_frame->document()->styleSelector()->styleRulesForElement(elt, authorOnly);
}

} // namespace WebCore
