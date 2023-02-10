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

#if ENABLE(MANAGED_MEDIA_SOURCE)

#include "MediaSource.h"

namespace WebCore {

class ManagedMediaSource final : public MediaSource {
    WTF_MAKE_ISO_ALLOCATED(ManagedMediaSource);
public:
    static Ref<ManagedMediaSource> create(ScriptExecutionContext&);
    ~ManagedMediaSource();

    enum class BufferingPolicy { Low, Medium, High };
    ExceptionOr<BufferingPolicy> buffering() const;

    enum class PreferredQuality { Low, Medium, High };
    ExceptionOr<PreferredQuality> quality() const;

    static bool isTypeSupported(ScriptExecutionContext&, const String& type);

    bool isManaged() const final { return true; }
private:
    explicit ManagedMediaSource(ScriptExecutionContext&);
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ManagedMediaSource)
    static bool isType(const WebCore::MediaSource& mediaSource) { return mediaSource.isManaged(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif
