/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#include "config.h"
#include "TextEvent.h"

#include "DocumentFragment.h"
#include "Editor.h"
#include "EventNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(TextEvent);

Ref<TextEvent> TextEvent::createForBindings()
{
    return adoptRef(*new TextEvent);
}

Ref<TextEvent> TextEvent::create(RefPtr<WindowProxy>&& view, const String& data, TextEventInputType inputType)
{
    return adoptRef(*new TextEvent(WTFMove(view), data, inputType));
}

Ref<TextEvent> TextEvent::createForPlainTextPaste(RefPtr<WindowProxy>&& view, const String& data, bool shouldSmartReplace)
{
    return adoptRef(*new TextEvent(WTFMove(view), data, 0, shouldSmartReplace, false, MailBlockquoteHandling::RespectBlockquote));
}

Ref<TextEvent> TextEvent::createForFragmentPaste(RefPtr<WindowProxy>&& view, RefPtr<DocumentFragment>&& data, bool shouldSmartReplace, bool shouldMatchStyle, MailBlockquoteHandling mailBlockquoteHandling)
{
    return adoptRef(*new TextEvent(WTFMove(view), emptyString(), WTFMove(data), shouldSmartReplace, shouldMatchStyle, mailBlockquoteHandling));
}

Ref<TextEvent> TextEvent::createForDrop(RefPtr<WindowProxy>&& view, const String& data)
{
    return adoptRef(*new TextEvent(WTFMove(view), data, TextEventInputDrop));
}

Ref<TextEvent> TextEvent::createForDictation(RefPtr<WindowProxy>&& view, const String& data, const Vector<DictationAlternative>& dictationAlternatives)
{
    return adoptRef(*new TextEvent(WTFMove(view), data, dictationAlternatives));
}

TextEvent::TextEvent()
    : m_inputType(TextEventInputKeyboard)
    , m_shouldSmartReplace(false)
    , m_shouldMatchStyle(false)
    , m_mailBlockquoteHandling(MailBlockquoteHandling::RespectBlockquote)
{
}

TextEvent::TextEvent(RefPtr<WindowProxy>&& view, const String& data, TextEventInputType inputType)
    : UIEvent(eventNames().textInputEvent, CanBubble::Yes, IsCancelable::Yes, IsComposed::Yes, WTFMove(view), 0)
    , m_inputType(inputType)
    , m_data(data)
    , m_shouldSmartReplace(false)
    , m_shouldMatchStyle(false)
    , m_mailBlockquoteHandling(MailBlockquoteHandling::RespectBlockquote)
{
}

TextEvent::TextEvent(RefPtr<WindowProxy>&& view, const String& data, RefPtr<DocumentFragment>&& pastingFragment, bool shouldSmartReplace, bool shouldMatchStyle, MailBlockquoteHandling mailBlockquoteHandling)
    : UIEvent(eventNames().textInputEvent, CanBubble::Yes, IsCancelable::Yes, IsComposed::Yes, WTFMove(view), 0)
    , m_inputType(TextEventInputPaste)
    , m_data(data)
    , m_pastingFragment(WTFMove(pastingFragment))
    , m_shouldSmartReplace(shouldSmartReplace)
    , m_shouldMatchStyle(shouldMatchStyle)
    , m_mailBlockquoteHandling(mailBlockquoteHandling)
{
}

TextEvent::TextEvent(RefPtr<WindowProxy>&& view, const String& data, const Vector<DictationAlternative>& dictationAlternatives)
    : UIEvent(eventNames().textInputEvent, CanBubble::Yes, IsCancelable::Yes, IsComposed::Yes, WTFMove(view), 0)
    , m_inputType(TextEventInputDictation)
    , m_data(data)
    , m_shouldSmartReplace(false)
    , m_shouldMatchStyle(false)
    , m_mailBlockquoteHandling(MailBlockquoteHandling::RespectBlockquote)
    , m_dictationAlternatives(dictationAlternatives)
{
}

TextEvent::~TextEvent() = default;

void TextEvent::initTextEvent(const AtomString& type, bool canBubble, bool cancelable, RefPtr<WindowProxy>&& view, const String& data)
{
    if (isBeingDispatched())
        return;

    initUIEvent(type, canBubble, cancelable, WTFMove(view), 0);

    m_inputType = TextEventInputKeyboard;

    m_data = data;

    m_pastingFragment = nullptr;
    m_shouldSmartReplace = false;
    m_shouldMatchStyle = false;
    m_mailBlockquoteHandling = MailBlockquoteHandling::RespectBlockquote;
    m_dictationAlternatives = { };
}

EventInterface TextEvent::eventInterface() const
{
    return TextEventInterfaceType;
}

bool TextEvent::isTextEvent() const
{
    return true;
}

} // namespace WebCore
