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

#pragma once

#include <wtf/TZoneMallocInlines.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/URL.h>
#include "DOMAudioSession.h"
#include "StringifyThis"

namespace WebCore {

struct TestSyncSerializationData;

class TestSyncData : public RefCounted<TestSyncData> {
WTF_MAKE_TZONE_ALLOCATED_INLINE(TestSyncData);
public:
    template<typename... Args>
    static Ref<TestSyncData> create(Args&&... args)
    {
        return adoptRef(*new TestSyncData(std::forward<Args>(args)...));
    }
    static Ref<TestSyncData> create() { return adoptRef(*new TestSyncData); }
    void update(const TestSyncSerializationData&);

    URL mainFrameURLChange = { };
    bool isAutofocusProcessed = { };
    bool userDidInteractWithPage = { };
#if ENABLE(DOM_AUDIO_SESSION)
    WebCore::DOMAudioSessionType audioSessionType = { };
#endif
    StringifyThis anotherOne = { };

private:
    TestSyncData() = default;
    WEBCORE_EXPORT TestSyncData(
        URL
      , bool
      , bool
#if ENABLE(DOM_AUDIO_SESSION)
      , WebCore::DOMAudioSessionType
#endif
      , StringifyThis
    );
};

enum class TestSyncDataType : uint8_t {
#if ENABLE(DOM_AUDIO_SESSION)
    AudioSessionType = 0,
#endif
    MainFrameURLChange = 1,
    IsAutofocusProcessed = 2,
    UserDidInteractWithPage = 3,
    AnotherOne = 4,
};

static const TestSyncDataType allTestSyncDataTypes[] = {
    TestSyncDataType::MainFrameURLChange
    , TestSyncDataType::IsAutofocusProcessed
    , TestSyncDataType::UserDidInteractWithPage
#if ENABLE(DOM_AUDIO_SESSION)
    , TestSyncDataType::AudioSessionType
#endif
    , TestSyncDataType::AnotherOne
};

#if !ENABLE(DOM_AUDIO_SESSION)
using DOMAudioSessionType = bool;
#endif

using TestSyncDataVariant = Variant<
    WebCore::DOMAudioSessionType,
    URL,
    bool,
    bool,
    StringifyThis
>;

struct TestSyncSerializationData {
    TestSyncDataType type;
    TestSyncDataVariant value;
};


} // namespace WebCore
