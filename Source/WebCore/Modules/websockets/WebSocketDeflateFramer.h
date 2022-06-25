/*
 * Copyright (C) 2012 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "WebSocketDeflater.h"
#include "WebSocketExtensionProcessor.h"

namespace WebCore {

class WebSocketDeflateFramer;
class WebSocketExtensionProcessor;

struct WebSocketFrame;

class DeflateResultHolder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit DeflateResultHolder(WebSocketDeflateFramer&);
    ~DeflateResultHolder();

    bool succeeded() const { return m_succeeded; }
    String failureReason() const { return m_failureReason; }

    void fail(const String& failureReason);

private:
    WebSocketDeflateFramer& m_framer;
    bool m_succeeded { true };
    String m_failureReason;
};

class InflateResultHolder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit InflateResultHolder(WebSocketDeflateFramer&);
    ~InflateResultHolder();

    bool succeeded() const { return m_succeeded; }
    String failureReason() const { return m_failureReason; }

    void fail(const String& failureReason);

private:
    WebSocketDeflateFramer& m_framer;
    bool m_succeeded { true };
    String m_failureReason;
};

class WebSocketDeflateFramer {
public:
    std::unique_ptr<WebSocketExtensionProcessor> createExtensionProcessor();

    bool enabled() const { return m_enabled; }

    std::unique_ptr<DeflateResultHolder> deflate(WebSocketFrame&);
    void resetDeflateContext();
    std::unique_ptr<InflateResultHolder> inflate(WebSocketFrame&);
    void resetInflateContext();

    void didFail();

    void enableDeflate(int windowBits, WebSocketDeflater::ContextTakeOverMode);

private:
    bool m_enabled { false };
    std::unique_ptr<WebSocketDeflater> m_deflater;
    std::unique_ptr<WebSocketInflater> m_inflater;
};

} // namespace WebCore
