/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include "MockBox.h"

#if ENABLE(MEDIA_SOURCE)

#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/DataView.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/Int8Array.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

MockBox::MockBox(ArrayBuffer* data)
{
    m_type = peekType(data);
    m_length = peekLength(data);
    ASSERT(m_length >= 8);
}

String MockBox::peekType(ArrayBuffer* data)
{
    StringBuilder builder;
    auto array = JSC::Int8Array::create(data, 0, 4);
    for (int i = 0; i < 4; ++i)
        builder.append(static_cast<char>(array->item(i)));
    return builder.toString();
}

size_t MockBox::peekLength(ArrayBuffer* data)
{
    auto view = JSC::DataView::create(data, 0, data->byteLength());
    return view->get<uint32_t>(4, true);
}

MockTrackBox::MockTrackBox(ArrayBuffer* data)
    : MockBox(data)
{
    ASSERT(m_length == 17);

    auto view = JSC::DataView::create(data, 0, data->byteLength());
    m_trackID = view->get<int32_t>(8, true);

    StringBuilder builder;
    auto array = JSC::Int8Array::create(data, 12, 4);
    for (int i = 0; i < 4; ++i)
        builder.append(static_cast<char>(array->item(i)));
    m_codec = builder.toString();

    m_kind = static_cast<TrackKind>(view->get<uint8_t>(16, true));
}

const String& MockTrackBox::type()
{
    static NeverDestroyed<String> trak(MAKE_STATIC_STRING_IMPL("trak"));
    return trak;
}

MockInitializationBox::MockInitializationBox(ArrayBuffer* data)
    : MockBox(data)
{
    ASSERT(m_length >= 13);

    auto view = JSC::DataView::create(data, 0, data->byteLength());
    int32_t timeValue = view->get<int32_t>(8, true);
    int32_t timeScale = view->get<int32_t>(12, true);
    m_duration = MediaTime(timeValue, timeScale);
    
    size_t offset = 16;

    while (offset < m_length) {
        auto subBuffer = data->slice(offset);
        if (MockBox::peekType(subBuffer.get()) != MockTrackBox::type())
            break;

        MockTrackBox trackBox(subBuffer.get());
        offset += trackBox.length();
        m_tracks.append(trackBox);
    }
}

const String& MockInitializationBox::type()
{
    static NeverDestroyed<String> init(MAKE_STATIC_STRING_IMPL("init"));
    return init;
}

MockSampleBox::MockSampleBox(ArrayBuffer* data)
    : MockBox(data)
{
    ASSERT(m_length == 30);

    auto view = JSC::DataView::create(data, 0, data->byteLength());
    int32_t timeScale = view->get<int32_t>(8, true);

    int32_t timeValue = view->get<int32_t>(12, true);
    m_presentationTimestamp = MediaTime(timeValue, timeScale);

    timeValue = view->get<int32_t>(16, true);
    m_decodeTimestamp = MediaTime(timeValue, timeScale);

    timeValue = view->get<int32_t>(20, true);
    m_duration = MediaTime(timeValue, timeScale);

    m_trackID = view->get<int32_t>(24, true);
    m_flags = view->get<uint8_t>(28, true);
    m_generation = view->get<uint8_t>(29, true);
}

const String& MockSampleBox::type()
{
    static NeverDestroyed<String> smpl(MAKE_STATIC_STRING_IMPL("smpl"));
    return smpl;
}

}

#endif
