/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef RTCDataChannelDescriptor_h
#define RTCDataChannelDescriptor_h

#if ENABLE(MEDIA_STREAM)

#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class RTCDataChannelDescriptorClient {
public:
    virtual ~RTCDataChannelDescriptorClient() { }

    virtual void readyStateChanged() = 0;
    virtual void dataArrived(const String&) = 0;
    virtual void dataArrived(const char*, size_t) = 0;
    virtual void error() = 0;
};

class RTCDataChannelDescriptor : public RefCounted<RTCDataChannelDescriptor> {
public:
    class ExtraData : public RefCounted<ExtraData> {
    public:
        virtual ~ExtraData() { }
    };

    enum ReadyState {
        ReadyStateConnecting = 0,
        ReadyStateOpen = 1,
        ReadyStateClosing = 2,
        ReadyStateClosed = 3,
    };

    static PassRefPtr<RTCDataChannelDescriptor> create(const String& label, bool reliable);
    virtual ~RTCDataChannelDescriptor();

    RTCDataChannelDescriptorClient* client() const { return m_client; }
    void setClient(RTCDataChannelDescriptorClient* client) { m_client = client; }

    const String& label() const { return m_label; }
    bool reliable() const { return m_reliable; }

    ReadyState readyState() const { return m_readyState; }

    unsigned long bufferedAmount() const { return m_bufferedAmount; }
    void setBufferedAmount(unsigned long bufferedAmount) { m_bufferedAmount = bufferedAmount; }

    void readyStateChanged(ReadyState);
    void dataArrived(const String&);
    void dataArrived(const char*, size_t);
    void error();

    PassRefPtr<ExtraData> extraData() const { return m_extraData; }
    void setExtraData(PassRefPtr<ExtraData> extraData) { m_extraData = extraData; }

private:
    RTCDataChannelDescriptor(const String& label, bool reliable);

    RTCDataChannelDescriptorClient* m_client;
    String m_label;
    bool m_reliable;
    ReadyState m_readyState;
    unsigned long m_bufferedAmount;
    RefPtr<ExtraData> m_extraData;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // RTCDataChannelDescriptor_h
