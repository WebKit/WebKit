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

namespace WebCore {

void TestSyncData::update(const TestSyncSerializationData& data)
{
    switch (data.type) {
    case TestSyncDataType::MainFrameURLChange:
        mainFrameURLChange = std::get<enumToUnderlyingType(TestSyncDataType::MainFrameURLChange)>(data.value);
        break;
    case TestSyncDataType::IsAutofocusProcessed:
        isAutofocusProcessed = std::get<enumToUnderlyingType(TestSyncDataType::IsAutofocusProcessed)>(data.value);
        break;
    case TestSyncDataType::UserDidInteractWithPage:
        userDidInteractWithPage = std::get<enumToUnderlyingType(TestSyncDataType::UserDidInteractWithPage)>(data.value);
        break;
#if ENABLE(DOM_AUDIO_SESSION)
    case TestSyncDataType::AudioSessionType:
        audioSessionType = std::get<enumToUnderlyingType(TestSyncDataType::AudioSessionType)>(data.value);
        break;
#endif
    case TestSyncDataType::AnotherOne:
        anotherOne = std::get<enumToUnderlyingType(TestSyncDataType::AnotherOne)>(data.value);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

TestSyncData::TestSyncData(
      URL mainFrameURLChange
    , bool isAutofocusProcessed
    , bool userDidInteractWithPage
#if ENABLE(DOM_AUDIO_SESSION)
    , WebCore::DOMAudioSessionType audioSessionType
#endif
    , StringifyThis anotherOne
)
    : mainFrameURLChange(mainFrameURLChange)
    , isAutofocusProcessed(isAutofocusProcessed)
    , userDidInteractWithPage(userDidInteractWithPage)
#if ENABLE(DOM_AUDIO_SESSION)
    , audioSessionType(audioSessionType)
#endif
    , anotherOne(anotherOne)
{
}

} // namespace WebCore
