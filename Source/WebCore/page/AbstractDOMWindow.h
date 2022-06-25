/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#include "Base64Utilities.h"
#include "EventTarget.h"
#include "GlobalWindowIdentifier.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class AbstractFrame;

// FIXME: Rename DOMWindow to LocalWindow and AbstractDOMWindow to DOMWindow.
class AbstractDOMWindow : public RefCounted<AbstractDOMWindow>, public EventTargetWithInlineData {
    WTF_MAKE_ISO_ALLOCATED(AbstractDOMWindow);
public:
    virtual ~AbstractDOMWindow();

    static HashMap<GlobalWindowIdentifier, AbstractDOMWindow*>& allWindows();

    const GlobalWindowIdentifier& identifier() const { return m_identifier; }

    virtual AbstractFrame* frame() const = 0;

    virtual bool isLocalDOMWindow() const = 0;
    virtual bool isRemoteDOMWindow() const = 0;

    using RefCounted::ref;
    using RefCounted::deref;

protected:
    explicit AbstractDOMWindow(GlobalWindowIdentifier&&);

    EventTargetInterface eventTargetInterface() const final { return DOMWindowEventTargetInterfaceType; }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

private:
    GlobalWindowIdentifier m_identifier;
};

} // namespace WebCore
