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

#include "config.h"
#include "TextFieldDecorationElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "ElementShadow.h"
#include "Event.h"
#include "HTMLInputElement.h"
#include "HTMLShadowElement.h"
#include "NodeRenderStyle.h"
#include "RenderImage.h"
#include "ShadowRoot.h"

namespace WebCore {

using namespace HTMLNames;

// TextFieldDecorator ----------------------------------------------------------------

TextFieldDecorator::~TextFieldDecorator()
{
}

// TextFieldDecorationElement ----------------------------------------------------------------

TextFieldDecorationElement::TextFieldDecorationElement(Document* document, TextFieldDecorator* decorator)
    : HTMLDivElement(HTMLNames::divTag, document)
    , m_textFieldDecorator(decorator)
{
    ASSERT(decorator);
    setHasCustomCallbacks();
}

PassRefPtr<TextFieldDecorationElement> TextFieldDecorationElement::create(Document* document, TextFieldDecorator* decorator)
{
    return adoptRef(new TextFieldDecorationElement(document, decorator));
}

static inline void getDecorationRootAndDecoratedRoot(HTMLInputElement* input, ShadowRoot*& decorationRoot, ShadowRoot*& decoratedRoot)
{
    ShadowRoot* existingRoot = input->shadow()->youngestShadowRoot();
    ShadowRoot* newRoot = 0;
    while (existingRoot->childNodeCount() == 1 && existingRoot->firstChild()->hasTagName(shadowTag)) {
        newRoot = existingRoot;
        existingRoot = existingRoot->olderShadowRoot();
        ASSERT(existingRoot);
    }
    if (newRoot)
        newRoot->removeChild(newRoot->firstChild());
    else
        newRoot = ShadowRoot::create(input, ShadowRoot::CreatingUserAgentShadowRoot, ASSERT_NO_EXCEPTION).get();
    decorationRoot = newRoot;
    decoratedRoot = existingRoot;
}

void TextFieldDecorationElement::decorate(HTMLInputElement* input)
{
    ASSERT(input);
    ShadowRoot* existingRoot;
    ShadowRoot* decorationRoot;
    getDecorationRootAndDecoratedRoot(input, decorationRoot, existingRoot);
    ASSERT(decorationRoot);
    ASSERT(existingRoot);
    RefPtr<HTMLDivElement> box = HTMLDivElement::create(input->document());
    decorationRoot->appendChild(box);
    box->setInlineStyleProperty(CSSPropertyDisplay, CSSValueWebkitBox);
    box->setInlineStyleProperty(CSSPropertyWebkitBoxAlign, CSSValueCenter);
    ASSERT(existingRoot->childNodeCount() == 1);
    toHTMLElement(existingRoot->firstChild())->setInlineStyleProperty(CSSPropertyWebkitBoxFlex, 1.0, CSSPrimitiveValue::CSS_NUMBER);
    box->appendChild(HTMLShadowElement::create(HTMLNames::shadowTag, input->document()));

    setInlineStyleProperty(CSSPropertyWebkitBoxFlex, 0.0, CSSPrimitiveValue::CSS_NUMBER);
    box->appendChild(this);
}

inline HTMLInputElement* TextFieldDecorationElement::hostInput()
{
    ASSERT(shadowAncestorNode());
    ASSERT(shadowAncestorNode()->hasTagName(inputTag));
    return static_cast<HTMLInputElement*>(shadowAncestorNode());
}

bool TextFieldDecorationElement::isTextFieldDecoration() const
{
    return true;
}

void TextFieldDecorationElement::updateImage()
{
    if (!renderer() || !renderer()->isImage())
        return;
    RenderImageResource* resource = toRenderImage(renderer())->imageResource();
    CachedImage* image;
    if (hostInput()->disabled())
        image = m_textFieldDecorator->imageForDisabledState();
    else if (hostInput()->readOnly())
        image = m_textFieldDecorator->imageForReadonlyState();
    else
        image = m_textFieldDecorator->imageForNormalState();
    ASSERT(image);
    resource->setCachedImage(image);
}

PassRefPtr<RenderStyle> TextFieldDecorationElement::customStyleForRenderer()
{
    RefPtr<RenderStyle> style = RenderStyle::create();
    RenderStyle* inputStyle = hostInput()->renderStyle();
    ASSERT(inputStyle);
    style->setWidth(Length(inputStyle->fontSize(), Fixed));
    style->setHeight(Length(inputStyle->fontSize(), Fixed));
    updateImage();
    return style.release();
}

RenderObject* TextFieldDecorationElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    RenderImage* image = new (arena) RenderImage(this);
    image->setImageResource(RenderImageResource::create());
    return image;
}

void TextFieldDecorationElement::attach()
{
    HTMLDivElement::attach();
    updateImage();
}

void TextFieldDecorationElement::detach()
{
    m_textFieldDecorator->willDetach(hostInput());
    HTMLDivElement::detach();
}

bool TextFieldDecorationElement::isMouseFocusable() const
{
    return false;
}

void TextFieldDecorationElement::defaultEventHandler(Event* event)
{
    RefPtr<HTMLInputElement> input(hostInput());
    if (input->disabled() || input->readOnly() || !event->isMouseEvent()) {
        if (!event->defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }

    RefPtr<TextFieldDecorationElement> protector(this);
    if (event->type() == eventNames().clickEvent) {
        m_textFieldDecorator->handleClick(input.get());
        event->setDefaultHandled();
    }

    if (!event->defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

} // namespace WebCore
