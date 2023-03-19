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
#include "ExceptionOr.h"
#include "LocalDOMWindowProperty.h"
#include "ScriptWrappable.h"

namespace WebCore {

class LocalDOMWindow;

class Location final : public ScriptWrappable, public RefCounted<Location>, public LocalDOMWindowProperty {
    WTF_MAKE_ISO_ALLOCATED(Location);
public:
    static Ref<Location> create(LocalDOMWindow& window) { return adoptRef(*new Location(window)); }

    ExceptionOr<void> setHref(LocalDOMWindow& incumbentWindow, LocalDOMWindow& firstWindow, const String&);
    String href() const;

    ExceptionOr<void> assign(LocalDOMWindow& activeWindow, LocalDOMWindow& firstWindow, const String&);
    ExceptionOr<void> replace(LocalDOMWindow& activeWindow, LocalDOMWindow& firstWindow, const String&);
    void reload(LocalDOMWindow& activeWindow);

    ExceptionOr<void> setProtocol(LocalDOMWindow& incumbentWindow, LocalDOMWindow& firstWindow, const String&);
    String protocol() const;
    ExceptionOr<void> setHost(LocalDOMWindow& incumbentWindow, LocalDOMWindow& firstWindow, const String&);
    WEBCORE_EXPORT String host() const;
    ExceptionOr<void> setHostname(LocalDOMWindow& incumbentWindow, LocalDOMWindow& firstWindow, const String&);
    String hostname() const;
    ExceptionOr<void> setPort(LocalDOMWindow& incumbentWindow, LocalDOMWindow& firstWindow, const String&);
    String port() const;
    ExceptionOr<void> setPathname(LocalDOMWindow& incumbentWindow, LocalDOMWindow& firstWindow, const String&);
    String pathname() const;
    ExceptionOr<void> setSearch(LocalDOMWindow& incumbentWindow, LocalDOMWindow& firstWindow, const String&);
    String search() const;
    ExceptionOr<void> setHash(LocalDOMWindow& incumbentWindow, LocalDOMWindow& firstWindow, const String&);
    String hash() const;
    String origin() const;

    String toString() const { return href(); }

    Ref<DOMStringList> ancestorOrigins() const;

private:
    explicit Location(LocalDOMWindow&);

    ExceptionOr<void> setLocation(LocalDOMWindow& incumbentWindow, LocalDOMWindow& firstWindow, const String&);

    const URL& url() const;
};

} // namespace WebCore
