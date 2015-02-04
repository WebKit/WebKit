/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ISOVTTCue.h"

#include "Logging.h"
#include <runtime/ArrayBuffer.h>
#include <runtime/DataView.h>
#include <runtime/Int8Array.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {
class ArrayBuffer;
}

namespace WebCore {

ISOBox::ISOBox(ArrayBuffer* data)
{
    m_type = peekType(data);
    m_length = peekLength(data);
    ASSERT(m_length >= 8);
}

String ISOBox::peekType(ArrayBuffer* data)
{
    ASSERT_WITH_SECURITY_IMPLICATION(data->byteLength() >= 2 * sizeof(uint32_t));
    if (data->byteLength() < 2 * sizeof(uint32_t))
        return emptyString();

    StringBuilder builder;
    RefPtr<Int8Array> array = JSC::Int8Array::create(data, 4, sizeof(uint32_t));
    for (int i = 0; i < 4; ++i)
        builder.append(array->item(i));
    return builder.toString();
}

size_t ISOBox::peekLength(ArrayBuffer* data)
{
    ASSERT_WITH_SECURITY_IMPLICATION(data->byteLength() >= sizeof(uint32_t));
    if (data->byteLength() < sizeof(uint32_t))
        return 0;

    return JSC::DataView::create(data, 0, sizeof(uint32_t))->get<uint32_t>(0, false);
}

String ISOBox::peekString(ArrayBuffer* data, unsigned offset, unsigned length)
{
    ASSERT_WITH_SECURITY_IMPLICATION(offset + length <= data->byteLength());
    if (data->byteLength() < offset + length)
        return emptyString();

    return String::fromUTF8(JSC::Uint8Array::create(data, offset, length)->data(), length);
}

static const AtomicString& vttCueBoxType()
{
    static NeverDestroyed<AtomicString> vttc("vttc", AtomicString::ConstructFromLiteral);
    return vttc;
}

static const AtomicString& vttIdBoxType()
{
    static NeverDestroyed<AtomicString> iden("iden", AtomicString::ConstructFromLiteral);
    return iden;
}

static const AtomicString& vttSettingsBoxType()
{
    static NeverDestroyed<AtomicString> sttg("sttg", AtomicString::ConstructFromLiteral);
    return sttg;
}

static const AtomicString& vttPayloadBoxType()
{
    static NeverDestroyed<AtomicString> payl("payl", AtomicString::ConstructFromLiteral);
    return payl;
}

static const AtomicString& vttCurrentTimeBoxType()
{
    static NeverDestroyed<AtomicString> ctim("ctim", AtomicString::ConstructFromLiteral);
    return ctim;
}

static const AtomicString& vttCueSourceIDBoxType()
{
    static NeverDestroyed<AtomicString> vsid("vsid", AtomicString::ConstructFromLiteral);
    return vsid;
}

const AtomicString& ISOWebVTTCue::boxType()
{
    return vttCueBoxType();
}

ISOWebVTTCue::ISOWebVTTCue(const MediaTime& presentationTime, const MediaTime& duration, JSC::ArrayBuffer* buffer)
    : ISOBox(buffer)
    , m_presentationTime(presentationTime)
    , m_duration(duration)
{
    size_t offset = ISOBox::boxHeaderSize();
    while (offset < length() && length() - offset > ISOBox::boxHeaderSize()) {
        RefPtr<ArrayBuffer> subBuffer = buffer->slice(offset);
        String boxType = ISOBox::peekType(subBuffer.get());
        size_t boxSize = ISOBox::peekLength(subBuffer.get());
        size_t boxDataSize = boxSize - ISOBox::boxHeaderSize();

        if (boxType == vttCueSourceIDBoxType())
            m_sourceID = peekString(subBuffer.get(), ISOBox::boxHeaderSize(), boxDataSize);
        else if (boxType == vttIdBoxType())
            m_identifer = peekString(subBuffer.get(), ISOBox::boxHeaderSize(), boxDataSize);
        else if (boxType == vttCurrentTimeBoxType())
            m_originalStartTime = peekString(subBuffer.get(), ISOBox::boxHeaderSize(), boxDataSize);
        else if (boxType == vttSettingsBoxType())
            m_settings = peekString(subBuffer.get(), ISOBox::boxHeaderSize(), boxDataSize);
        else if (boxType == vttPayloadBoxType())
            m_cueText = peekString(subBuffer.get(), ISOBox::boxHeaderSize(), boxDataSize);
        else
            LOG(Media, "ISOWebVTTCue::ISOWebVTTCue - skipping box id = \"%s\", size = %zu", boxType.utf8().data(), boxSize);

        offset += boxSize;
    }
}

} // namespace WebCore
