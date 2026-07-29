/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "TestSyncData.h"

#include <wtf/EnumTraits.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

void TestSyncData::update(const TestSyncSerializationData& data)
{
    switch (static_cast<TestSyncDataType>(data.value.index())) {
    case TestSyncDataType::MainFrameURLChange:
        mainFrameURLChange = std::get<std::to_underlying(TestSyncDataType::MainFrameURLChange)>(data.value);
        break;
    case TestSyncDataType::IsAutofocusProcessed:
        isAutofocusProcessed = std::get<std::to_underlying(TestSyncDataType::IsAutofocusProcessed)>(data.value);
        break;
    case TestSyncDataType::UserDidInteractWithPage:
        userDidInteractWithPage = std::get<std::to_underlying(TestSyncDataType::UserDidInteractWithPage)>(data.value);
        break;
    case TestSyncDataType::AnotherOne:
        anotherOne = std::get<std::to_underlying(TestSyncDataType::AnotherOne)>(data.value);
        break;
#if ENABLE(DOM_AUDIO_SESSION)
    case TestSyncDataType::AudioSessionType:
        audioSessionType = std::get<std::to_underlying(TestSyncDataType::AudioSessionType)>(data.value);
        break;
#endif
    case TestSyncDataType::MultipleHeaders:
        multipleHeaders = std::get<std::to_underlying(TestSyncDataType::MultipleHeaders)>(data.value);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

TestSyncData::TestSyncData(
      URL mainFrameURLChange
    , bool isAutofocusProcessed
    , bool userDidInteractWithPage
    , StringifyThis anotherOne
#if ENABLE(DOM_AUDIO_SESSION)
    , WebCore::DOMAudioSessionType audioSessionType
#endif
    , HashSet<URL> multipleHeaders
)
    : mainFrameURLChange(mainFrameURLChange)
    , isAutofocusProcessed(isAutofocusProcessed)
    , userDidInteractWithPage(userDidInteractWithPage)
    , anotherOne(anotherOne)
#if ENABLE(DOM_AUDIO_SESSION)
    , audioSessionType(audioSessionType)
#endif
    , multipleHeaders(multipleHeaders)
{
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const TestSyncData& data)
{
    WTF::TextStream::GroupScope scope(ts);
    ts << "TestSyncData"_s;
    ts.dumpProperty("mainFrameURLChange"_s, ValueOrEllipsis(data.mainFrameURLChange));
    ts.dumpProperty("isAutofocusProcessed"_s, ValueOrEllipsis(data.isAutofocusProcessed));
    ts.dumpProperty("userDidInteractWithPage"_s, ValueOrEllipsis(data.userDidInteractWithPage));
    ts.dumpProperty("anotherOne"_s, ValueOrEllipsis(data.anotherOne));
#if ENABLE(DOM_AUDIO_SESSION)
    ts.dumpProperty("audioSessionType"_s, ValueOrEllipsis(data.audioSessionType));
#endif
    ts.dumpProperty("multipleHeaders"_s, ValueOrEllipsis(data.multipleHeaders));
    return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const TestSyncSerializationData& data)
{
    WTF::TextStream::GroupScope scope(ts);
    ts << "TestSyncSerializationData"_s;
    switch (static_cast<TestSyncDataType>(data.value.index())) {
    case TestSyncDataType::MainFrameURLChange:
        ts.dumpProperty("mainFrameURLChange"_s, ValueOrEllipsis(std::get<std::to_underlying(TestSyncDataType::MainFrameURLChange)>(data.value)));
        break;
    case TestSyncDataType::IsAutofocusProcessed:
        ts.dumpProperty("isAutofocusProcessed"_s, ValueOrEllipsis(std::get<std::to_underlying(TestSyncDataType::IsAutofocusProcessed)>(data.value)));
        break;
    case TestSyncDataType::UserDidInteractWithPage:
        ts.dumpProperty("userDidInteractWithPage"_s, ValueOrEllipsis(std::get<std::to_underlying(TestSyncDataType::UserDidInteractWithPage)>(data.value)));
        break;
    case TestSyncDataType::AnotherOne:
        ts.dumpProperty("anotherOne"_s, ValueOrEllipsis(std::get<std::to_underlying(TestSyncDataType::AnotherOne)>(data.value)));
        break;
#if ENABLE(DOM_AUDIO_SESSION)
    case TestSyncDataType::AudioSessionType:
        ts.dumpProperty("audioSessionType"_s, ValueOrEllipsis(std::get<std::to_underlying(TestSyncDataType::AudioSessionType)>(data.value)));
        break;
#endif
    case TestSyncDataType::MultipleHeaders:
        ts.dumpProperty("multipleHeaders"_s, ValueOrEllipsis(std::get<std::to_underlying(TestSyncDataType::MultipleHeaders)>(data.value)));
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    return ts;
}

} // namespace WebCore
