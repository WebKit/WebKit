/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef TextFieldDecorationElement_h
#define TextFieldDecorationElement_h

#include "HTMLDivElement.h"

namespace WebCore {

class CachedImage;
class HTMLInputElement;

// A TextFieldDecorator object must live until all of text fields which were
// decorated by it die.
class TextFieldDecorator {
public:
    // Returns true if this TextFieldDecorator wants to add a
    // decoration to the specified text field.
    virtual bool willAddDecorationTo(HTMLInputElement*) = 0;

    // A TextFieldDecorator object should own the CachedImage objects.
    virtual CachedImage* imageForNormalState() = 0;
    virtual CachedImage* imageForDisabledState() = 0;
    virtual CachedImage* imageForReadonlyState() = 0;

    virtual void handleClick(HTMLInputElement*) = 0;
    // This function is called just before detaching the decoration. It must not
    // call functions which updating state of the specified HTMLInputElement
    // object.
    virtual void willDetach(HTMLInputElement*) = 0;

    virtual ~TextFieldDecorator();
};

// A TextFieldDecorationElement object must be in a shadow tree of an
// HTMLInputElement.
class TextFieldDecorationElement : public HTMLDivElement {
public:
    static PassRefPtr<TextFieldDecorationElement> create(Document*, TextFieldDecorator*);
    TextFieldDecorator* textFieldDecorator() { return m_textFieldDecorator; }
    void decorate(HTMLInputElement*);

private:
    TextFieldDecorationElement(Document*, TextFieldDecorator*);
    virtual bool isTextFieldDecoration() const OVERRIDE;
    virtual PassRefPtr<RenderStyle> customStyleForRenderer() OVERRIDE;
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*) OVERRIDE;
    virtual void attach() OVERRIDE;
    virtual void detach() OVERRIDE;
    virtual bool isMouseFocusable() const OVERRIDE;
    virtual void defaultEventHandler(Event*) OVERRIDE;

    HTMLInputElement* hostInput();
    void updateImage();

    TextFieldDecorator* m_textFieldDecorator;
};

inline TextFieldDecorationElement* toTextFieldDecorationElement(Node* node)
{
    ASSERT(node);
    ASSERT(node->isElementNode());
    ASSERT(static_cast<Element*>(node)->isTextFieldDecoration());
    return static_cast<TextFieldDecorationElement*>(node);
}

}
#endif
