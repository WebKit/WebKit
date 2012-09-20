/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "BaseButtonInputType.h"

#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "RenderButton.h"
#include "RenderTextFragment.h"
#include "ShadowRoot.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

class TextForButtonInputType : public Text {
public:
    static PassRefPtr<TextForButtonInputType> create(Document*, const String&);

private:
    TextForButtonInputType(Document*, const String&);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*) OVERRIDE;
};

PassRefPtr<TextForButtonInputType> TextForButtonInputType::create(Document* document, const String& data)
{
    return adoptRef(new TextForButtonInputType(document, data));
}

TextForButtonInputType::TextForButtonInputType(Document* document, const String& data)
    : Text(document, data)
{
}

RenderObject* TextForButtonInputType::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderTextFragment(document(), dataImpl());
}

BaseButtonInputType::BaseButtonInputType(HTMLInputElement* element)
    : BaseClickableWithKeyInputType(element)
{
}

void BaseButtonInputType::createShadowSubtree()
{
    ASSERT(element()->userAgentShadowRoot());

    RefPtr<TextForButtonInputType> text = TextForButtonInputType::create(element()->document(), defaultValue());
    element()->userAgentShadowRoot()->appendChild(text);
}

void BaseButtonInputType::destroyShadowSubtree()
{
    InputType::destroyShadowSubtree();
}

void BaseButtonInputType::valueAttributeChanged()
{
    String value = element()->valueWithDefault();
    toText(element()->userAgentShadowRoot()->firstChild())->setData(value, ASSERT_NO_EXCEPTION);
}

bool BaseButtonInputType::shouldSaveAndRestoreFormControlState() const
{
    return false;
}

bool BaseButtonInputType::appendFormData(FormDataList&, bool) const
{
    // Buttons except overridden types are never successful.
    return false;
}

RenderObject* BaseButtonInputType::createRenderer(RenderArena* arena, RenderStyle*) const
{
    return new (arena) RenderButton(element());
}

bool BaseButtonInputType::storesValueSeparateFromAttribute()
{
    return false;
}

void BaseButtonInputType::setValue(const String& sanitizedValue, bool, TextFieldEventBehavior)
{
    element()->setAttribute(valueAttr, sanitizedValue);
}

} // namespace WebCore
