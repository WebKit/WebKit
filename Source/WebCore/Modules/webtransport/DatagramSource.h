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

#include "ReadableStreamSource.h"
#include <wtf/AbstractRefCounted.h>

namespace JSC {
class JSGlobalObject;
class JSValue;
}

namespace WebCore {

class WebTransport;
class Exception;

class DatagramSource : public AbstractRefCounted {
public:
    DatagramSource() = default;
    virtual ~DatagramSource() = default;
    virtual void receiveDatagram(std::span<const uint8_t>, bool, std::optional<Exception>&&) = 0;
    virtual void error(JSC::JSGlobalObject&, JSC::JSValue) = 0;
};

class DatagramDefaultSource final : public DatagramSource, public RefCountedReadableStreamSource {
public:
    static Ref<DatagramDefaultSource> create() { return adoptRef(*new DatagramDefaultSource()); }
    ~DatagramDefaultSource();

    void ref() const final { return RefCountedReadableStreamSource::ref(); }
    void deref() const final { return RefCountedReadableStreamSource::deref(); }

private:
    DatagramDefaultSource();

    void receiveDatagram(std::span<const uint8_t>, bool, std::optional<Exception>&&) final;
    void error(JSC::JSGlobalObject&, JSC::JSValue) final;

    void setActive() final { }
    void setInactive() final { }
    void doStart() final { }
    void doPull() final { }
    void doCancel(JSC::JSValue) final { doCancel(); }

    void doCancel();

    bool m_isCancelled { false };
    bool m_isClosed { false };
};

}
