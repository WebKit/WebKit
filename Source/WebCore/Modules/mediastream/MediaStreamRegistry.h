/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "URLRegistry.h"
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class MediaStream;
class MediaStreamPrivate;

class MediaStreamRegistry final : public URLRegistry {
public:
    friend class NeverDestroyed<MediaStreamRegistry>;

    // Returns a single instance of MediaStreamRegistry.
    static MediaStreamRegistry& shared();

    // Registers a blob URL referring to the specified stream data.
    void registerURL(SecurityOrigin*, const URL&, URLRegistrable&) override;
    void unregisterURL(const URL&) override;

    URLRegistrable* lookup(const String&) const override;

    void registerStream(MediaStream&);
    void unregisterStream(MediaStream&);

    MediaStream* lookUp(const URL&) const;

    void forEach(const WTF::Function<void(MediaStream&)>&) const;

private:
    MediaStreamRegistry() = default;
    HashMap<String, RefPtr<MediaStream>> m_mediaStreams;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
