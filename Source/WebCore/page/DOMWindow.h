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

#include "EventTarget.h"
#include "GlobalWindowIdentifier.h"
#include <wtf/CheckedRef.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class Document;
class Frame;
class LocalDOMWindow;
class Location;
class PageConsoleClient;
class SecurityOrigin;
class WebCoreOpaqueRoot;
class WindowProxy;
enum class SetLocationLocking : bool { LockHistoryBasedOnGestureState, LockHistoryAndBackForwardList };

class DOMWindow : public RefCounted<DOMWindow>, public EventTarget {
    WTF_MAKE_ISO_ALLOCATED(DOMWindow);
public:
    virtual ~DOMWindow();

    const GlobalWindowIdentifier& identifier() const { return m_identifier; }
    virtual Frame* frame() const = 0;
    RefPtr<Frame> protectedFrame() const;

    virtual bool isLocalDOMWindow() const = 0;
    virtual bool isRemoteDOMWindow() const = 0;

    using RefCounted::ref;
    using RefCounted::deref;

    WEBCORE_EXPORT Location& location();
    virtual void setLocation(LocalDOMWindow& activeWindow, const URL& completedURL, SetLocationLocking = SetLocationLocking::LockHistoryBasedOnGestureState) = 0;

    bool closed() const;
    WEBCORE_EXPORT void close();
    void close(Document&);
    virtual void closePage() = 0;

    PageConsoleClient* console() const;
    CheckedPtr<PageConsoleClient> checkedConsole() const;

    WindowProxy* opener() const;

    WindowProxy* top() const;
    WindowProxy* parent() const;

protected:
    explicit DOMWindow(GlobalWindowIdentifier&&);

    ExceptionOr<RefPtr<SecurityOrigin>> createTargetOriginForPostMessage(const String&, Document&);

    EventTargetInterface eventTargetInterface() const final { return LocalDOMWindowEventTargetInterfaceType; }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

private:
    GlobalWindowIdentifier m_identifier;
    RefPtr<Location> m_location;
};

WebCoreOpaqueRoot root(DOMWindow*);

} // namespace WebCore
