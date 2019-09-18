/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InputEvent.h"

#include "DataTransfer.h"
#include "Node.h"
#include "WindowProxy.h"
#include <wtf/Vector.h>

namespace WebCore {

Ref<InputEvent> InputEvent::create(const AtomString& eventType, const String& inputType, IsCancelable cancelable, RefPtr<WindowProxy>&& view, const String& data, RefPtr<DataTransfer>&& dataTransfer, const Vector<RefPtr<StaticRange>>& targetRanges, int detail)
{
    return adoptRef(*new InputEvent(eventType, inputType, cancelable, WTFMove(view), data, WTFMove(dataTransfer), targetRanges, detail));
}

InputEvent::InputEvent(const AtomString& eventType, const String& inputType, IsCancelable cancelable, RefPtr<WindowProxy>&& view, const String& data, RefPtr<DataTransfer>&& dataTransfer, const Vector<RefPtr<StaticRange>>& targetRanges, int detail)
    : UIEvent(eventType, CanBubble::Yes, cancelable, IsComposed::Yes, WTFMove(view), detail)
    , m_inputType(inputType)
    , m_data(data)
    , m_dataTransfer(dataTransfer)
    , m_targetRanges(targetRanges)
{
}

InputEvent::InputEvent(const AtomString& eventType, const Init& initializer)
    : UIEvent(eventType, initializer)
    , m_inputType(emptyString())
    , m_data(initializer.data)
{
}

RefPtr<DataTransfer> InputEvent::dataTransfer() const
{
    return m_dataTransfer;
}

} // namespace WebCore
