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

#pragma once

#include "Pasteboard.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class SharedBuffer;

class StaticPasteboard final : public Pasteboard {
public:
    StaticPasteboard();
    ~StaticPasteboard();

    PasteboardCustomData takeCustomData();

    bool hasNonDefaultData() const;
    bool isStatic() const final { return true; }

    bool hasData() final;
    Vector<String> typesSafeForBindings(const String&) final;
    Vector<String> typesForLegacyUnsafeBindings() final;
    String readOrigin() final { return { }; }
    String readString(const String& type) final;
    String readStringInCustomData(const String& type) final;

    void writeString(const String& type, const String& data) final;
    void writeData(const String& type, Ref<SharedBuffer>&& data);
    void writeStringInCustomData(const String& type, const String& data);
    void clear() final;
    void clear(const String& type) final;

    void read(PasteboardPlainText&, PlainTextURLReadingPolicy = PlainTextURLReadingPolicy::AllowURL, std::optional<size_t> = std::nullopt) final { }
    void read(PasteboardWebContentReader&, WebContentReadingPolicy, std::optional<size_t> = std::nullopt) final { }

    void write(const PasteboardURL&) final;
    void write(const PasteboardImage&) final;
    void write(const PasteboardWebContent&) final;
    void writeMarkup(const String&) final;
    void writePlainText(const String&, SmartReplaceOption) final;

    void writeCustomData(const Vector<PasteboardCustomData>&) final { }

    Pasteboard::FileContentState fileContentState() final { return m_fileContentState; }
    bool canSmartReplace() final { return false; }

#if ENABLE(DRAG_SUPPORT)
    void setDragImage(DragImage, const IntPoint&) final { }
#endif

private:
    PasteboardCustomData m_customData;
    HashSet<String> m_nonDefaultDataTypes;
    Pasteboard::FileContentState m_fileContentState { Pasteboard::FileContentState::NoFileOrImageData };
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StaticPasteboard)
    static bool isType(const WebCore::Pasteboard& pasteboard) { return pasteboard.isStatic(); }
SPECIALIZE_TYPE_TRAITS_END()
