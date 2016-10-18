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

#if ENABLE(MEDIA_STREAM)

#include <wtf/text/WTFString.h>

namespace WebCore {

class CaptureDevice {
public:
    enum class SourceKind { Unknown, Audio, Video };

    CaptureDevice(const String& persistentId, SourceKind kind, const String& label, const String& groupId)
        : m_persistentId(persistentId)
        , m_kind(kind)
        , m_label(label)
        , m_groupId(groupId)
    {
    }

    CaptureDevice() = default;

    const String& persistentId() const { return m_persistentId; }
    void setPersistentId(const String& id) { m_persistentId = id; }

    const String& label() const { return m_label; }
    void setLabel(const String& label) { m_label = label; }

    const String& groupId() const { return m_groupId; }
    void setGroupId(const String& id) { m_groupId = id; }

    SourceKind kind() const { return m_kind; }
    void setKind(SourceKind kind) { m_kind = kind; }

private:
    String m_persistentId;
    SourceKind m_kind { SourceKind::Unknown };
    String m_label;
    String m_groupId;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
