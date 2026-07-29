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
#include "TestSyncClient.h"

#include "TestSyncData.h"
#include <wtf/EnumTraits.h>

namespace WebCore {

#if ENABLE(DOM_AUDIO_SESSION)
void TestSyncClient::broadcastAudioSessionTypeToOtherProcesses(const WebCore::DOMAudioSessionType& data)
{
    broadcastTestSyncDataToOtherProcesses({ TestSyncDataVariant { WTF::InPlaceIndex<std::to_underlying(TestSyncDataType::AudioSessionType)>, data } });
}
#endif
void TestSyncClient::broadcastMainFrameURLChangeToOtherProcesses(const URL& data)
{
    broadcastTestSyncDataToOtherProcesses({ TestSyncDataVariant { WTF::InPlaceIndex<std::to_underlying(TestSyncDataType::MainFrameURLChange)>, data } });
}
void TestSyncClient::broadcastIsAutofocusProcessedToOtherProcesses(const bool& data)
{
    broadcastTestSyncDataToOtherProcesses({ TestSyncDataVariant { WTF::InPlaceIndex<std::to_underlying(TestSyncDataType::IsAutofocusProcessed)>, data } });
}
void TestSyncClient::broadcastUserDidInteractWithPageToOtherProcesses(const bool& data)
{
    broadcastTestSyncDataToOtherProcesses({ TestSyncDataVariant { WTF::InPlaceIndex<std::to_underlying(TestSyncDataType::UserDidInteractWithPage)>, data } });
}
void TestSyncClient::broadcastAnotherOneToOtherProcesses(const StringifyThis& data)
{
    broadcastTestSyncDataToOtherProcesses({ TestSyncDataVariant { WTF::InPlaceIndex<std::to_underlying(TestSyncDataType::AnotherOne)>, data } });
}
void TestSyncClient::broadcastMultipleHeadersToOtherProcesses(const HashSet<URL>& data)
{
    broadcastTestSyncDataToOtherProcesses({ TestSyncDataVariant { WTF::InPlaceIndex<std::to_underlying(TestSyncDataType::MultipleHeaders)>, data } });
}

} // namespace WebCore
