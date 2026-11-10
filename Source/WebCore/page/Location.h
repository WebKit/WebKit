/*
 * Copyright (C) 2008, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "DOMStringList.h"
#include "EventTarget.h"
#include "ScriptWrappable.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class DOMWindow;
class Frame;
class LocalDOMWindow;
template<typename> class ExceptionOr;

class Location final : public ScriptWrappable, public RefCounted<Location> {
    WTF_MAKE_TZONE_ALLOCATED(Location);
public:
    static Ref<Location> create(DOMWindow& window) { return adoptRef(*new Location(window)); }

    WEBCORE_EXPORT ~Location();

    ExceptionOr<void> setHref(LocalDOMWindow& incumbentWindow, LocalDOMWindow& firstWindow, StringImpl*);
    RefPtr<StringImpl> href() const;

    ExceptionOr<void> assign(LocalDOMWindow& activeWindow, LocalDOMWindow& firstWindow, StringImpl*);
    ExceptionOr<void> replace(LocalDOMWindow& activeWindow, LocalDOMWindow& firstWindow, StringImpl*);
    void reload(LocalDOMWindow& activeWindow);

    ExceptionOr<void> setProtocol(LocalDOMWindow& incumbentWindow, LocalDOMWindow& firstWindow, StringImpl*);
    RefPtr<StringImpl> protocol() const;
    ExceptionOr<void> setHost(LocalDOMWindow& incumbentWindow, LocalDOMWindow& firstWindow, StringImpl*);
    WEBCORE_EXPORT RefPtr<StringImpl> host() const;
    ExceptionOr<void> setHostname(LocalDOMWindow& incumbentWindow, LocalDOMWindow& firstWindow, StringImpl*);
    RefPtr<StringImpl> hostname() const;
    ExceptionOr<void> setPort(LocalDOMWindow& incumbentWindow, LocalDOMWindow& firstWindow, StringImpl*);
    RefPtr<StringImpl> port() const;
    ExceptionOr<void> setPathname(LocalDOMWindow& incumbentWindow, LocalDOMWindow& firstWindow, StringImpl*);
    RefPtr<StringImpl> pathname() const;
    ExceptionOr<void> setSearch(LocalDOMWindow& incumbentWindow, LocalDOMWindow& firstWindow, StringImpl*);
    RefPtr<StringImpl> search() const;
    ExceptionOr<void> setHash(LocalDOMWindow& incumbentWindow, LocalDOMWindow& firstWindow, StringImpl*);
    RefPtr<StringImpl> hash() const;
    RefPtr<StringImpl> origin() const;

    RefPtr<StringImpl> toString() const { return href(); }

    Ref<DOMStringList> ancestorOrigins() const;
    DOMStringList* cachedAncestorOrigins() const { return m_ancestorOrigins.get(); }

    DOMWindow* window() { return m_window.get(); }

    const URL& url() const LIFETIME_BOUND;

private:
    explicit Location(DOMWindow&);

    ExceptionOr<void> setLocation(LocalDOMWindow& incumbentWindow, LocalDOMWindow& firstWindow, StringImpl*);

    NODELETE Frame* frame();
    NODELETE const Frame* frame() const;

    mutable RefPtr<DOMStringList> m_ancestorOrigins;
    WeakPtr<DOMWindow, WeakPtrImplWithEventTargetData> m_window;
};

} // namespace WebCore
