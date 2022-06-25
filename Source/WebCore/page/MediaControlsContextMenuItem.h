/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct MediaControlsContextMenuItem {
    using ID = uint64_t;
    static constexpr ID invalidID = 0;

    ID id { invalidID };
    String title;
    String icon;
    bool checked { false };
    Vector<MediaControlsContextMenuItem> children;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<MediaControlsContextMenuItem> decode(Decoder&);
};

template<class Encoder>
void MediaControlsContextMenuItem::encode(Encoder& encoder) const
{
    encoder << id;
    encoder << title;
    encoder << icon;
    encoder << checked;
    encoder << children;
}

template<class Decoder>
std::optional<MediaControlsContextMenuItem> MediaControlsContextMenuItem::decode(Decoder& decoder)
{
#define DECODE(name, type) \
    std::optional<type> name; \
    decoder >> name; \
    if (!name) \
        return std::nullopt; \

    DECODE(id, ID);
    DECODE(title, String);
    DECODE(icon, String);
    DECODE(checked, bool);
    DECODE(children, Vector<MediaControlsContextMenuItem>);

#undef DECODE

    return {{ WTFMove(*id), WTFMove(*title), WTFMove(*icon), WTFMove(*checked), WTFMove(*children) }};
}

} // namespace WebCore

#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
