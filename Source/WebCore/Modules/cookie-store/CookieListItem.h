/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "Cookie.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

struct CookieListItem {
    String name;
    String value;

    static CookieListItem fromCookie(Cookie&& cookie)
    {
        CookieListItem c = { WTF::move(cookie.name), WTF::move(cookie.value) };

#if OS(DARWIN)
        // On Cocoa, cookie names/values are stored as UTF-8 bytes reinterpreted as Latin-1
        // (see CookieStorageSessionCocoa.mm) because CFNetwork only preserves Latin-1
        // characters. CookieListItem is only used by the Cookie Store API in JS/DOM, so
        // reverse that trick here by force re-interpreting the bytes as UTF-8, allowing for
        // lossy conversion. Other platforms decode cookies as proper Unicode strings already.
        c.name = String::fromUTF8ReplacingInvalidSequences(c.name.span8());
        c.value = String::fromUTF8ReplacingInvalidSequences(c.value.span8());
#endif

        return c;
    }
};

} // namespace WebCore
