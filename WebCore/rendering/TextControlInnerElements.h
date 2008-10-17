/*
 * Copyright (C) 2006, 2008 Apple Inc.  All rights reserved.
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
 
#ifndef TextControlInnerElements_h
#define TextControlInnerElements_h

#include "HTMLDivElement.h"

namespace WebCore {

class String;

class TextControlInnerElement : public HTMLDivElement
{
public:
    TextControlInnerElement(Document*, Node* shadowParent = 0);
    
    virtual bool isMouseFocusable() const { return false; } 
    virtual bool isShadowNode() const { return m_shadowParent; }
    virtual Node* shadowParentNode() { return m_shadowParent; }
    void setShadowParentNode(Node* node) { m_shadowParent = node; }
    void attachInnerElement(Node*, PassRefPtr<RenderStyle>, RenderArena*);
    
private:
    Node* m_shadowParent;
};

class TextControlInnerTextElement : public TextControlInnerElement {
public:
    TextControlInnerTextElement(Document*, Node* shadowParent);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);  
    virtual void defaultEventHandler(Event*);
};

class SearchFieldResultsButtonElement : public TextControlInnerElement {
public:
    SearchFieldResultsButtonElement(Document*);
    virtual void defaultEventHandler(Event*);
};

class SearchFieldCancelButtonElement : public TextControlInnerElement {
public:
    SearchFieldCancelButtonElement(Document*);
    virtual void defaultEventHandler(Event*);
private:
    bool m_capturing;
};

} //namespace

#endif
