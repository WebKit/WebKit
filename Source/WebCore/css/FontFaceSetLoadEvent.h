/*
 * Copyright (C) 2018 Igalia S.L. All rights reserved.
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 */

#pragma once

#include "Event.h"
#include <wtf/Forward.h>

namespace WebCore {

class FontFace;
struct FontFaceSetLoadEventInit;

using FontFaceArray = Vector<Ref<FontFace>>;

class FontFaceSetLoadEvent final : public Event {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(FontFaceSetLoadEvent);
public:
    static Ref<FontFaceSetLoadEvent> create(const AtomString& type, const FontFaceArray& fontfaces = FontFaceArray());
    static Ref<FontFaceSetLoadEvent> create(const AtomString& type, const FontFaceSetLoadEventInit& eventInitDict, IsTrusted = IsTrusted::No);
    virtual ~FontFaceSetLoadEvent();

    FontFaceArray fontfaces() const { return m_fontFaces; }

private:
    bool isFontFaceSetLoadEvent() const final { return true; }

    FontFaceSetLoadEvent(const AtomString&, const FontFaceArray&);
    FontFaceSetLoadEvent(const AtomString&, const FontFaceSetLoadEventInit&, IsTrusted);
    FontFaceArray m_fontFaces;
};

} // namespace WebCore
