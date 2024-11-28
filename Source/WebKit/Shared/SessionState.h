/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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

#include "ViewSnapshotStore.h"
#include <WebCore/BackForwardItemIdentifier.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/IntRect.h>
#include <WebCore/PolicyContainer.h>
#include <WebCore/SerializedScriptValue.h>
#include <wtf/ArgumentCoder.h>
#include <wtf/RefCounted.h>
#include <wtf/RunLoop.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

struct HTTPBody {
    struct Element {
        struct FileData {
            String filePath;
            int64_t fileStart;
            std::optional<int64_t> fileLength;
            std::optional<WallTime> expectedFileModificationTime;
        };
        using BlobURLString = String;
        using Data = std::variant<Vector<uint8_t>, FileData, BlobURLString>;
        Data data;
    };

    String contentType;
    Vector<Element> elements;
};

class FrameState : public RefCounted<FrameState> {
public:
    template<typename... Args>
    static Ref<FrameState> create(Args&&... args)
    {
        return adoptRef(*new FrameState(std::forward<Args>(args)...));
    }

    // These are used to help debug <rdar://problem/48634553>.
    FrameState() { RELEASE_ASSERT(RunLoop::isMain()); }
    ~FrameState() { RELEASE_ASSERT(RunLoop::isMain()); }

    Ref<FrameState> copy();

    const Vector<AtomString>& documentState() const { return m_documentState; }
    enum class ShouldValidate : bool { No, Yes };
    void setDocumentState(const Vector<AtomString>&, ShouldValidate = ShouldValidate::No);
    static bool validateDocumentState(const Vector<AtomString>&);

    String urlString;
    String originalURLString;
    String referrer;
    AtomString target;
    std::optional<WebCore::FrameIdentifier> frameID;

    std::optional<Vector<uint8_t>> stateObjectData;

    int64_t documentSequenceNumber { 0 };
    int64_t itemSequenceNumber { 0 };

    WebCore::IntPoint scrollPosition;
    bool shouldRestoreScrollPosition { true };
    float pageScaleFactor { 0 };

    std::optional<HTTPBody> httpBody;

    Markable<WebCore::BackForwardItemIdentifier> identifier;
    bool hasCachedPage { false };
    String title;
    WebCore::ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy { WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow };
    RefPtr<WebCore::SerializedScriptValue> sessionStateObject;
    bool wasCreatedByJSWithoutUserInteraction { false };
    bool wasRestoredFromSession { false };
    std::optional<WebCore::PolicyContainer> policyContainer;

    // FIXME: These should not be per frame.
#if PLATFORM(IOS_FAMILY)
    WebCore::FloatRect exposedContentRect;
    WebCore::IntRect unobscuredContentRect;
    WebCore::FloatSize minimumLayoutSizeInScrollViewCoordinates;
    WebCore::IntSize contentSize;
    bool scaleIsInitial { false };
    WebCore::FloatBoxExtent obscuredInsets;
#endif

    Vector<Ref<FrameState>> children;

private:
    FrameState(const String& urlString, const String& originalURLString, const String& referrer, const AtomString& target, std::optional<WebCore::FrameIdentifier>, std::optional<Vector<uint8_t>> stateObjectData, int64_t documentSequenceNumber, int64_t itemSequenceNumber, WebCore::IntPoint scrollPosition, bool shouldRestoreScrollPosition, float pageScaleFactor, const std::optional<HTTPBody>&, std::optional<WebCore::BackForwardItemIdentifier>, bool hasCachedPage, const String& title, WebCore::ShouldOpenExternalURLsPolicy, RefPtr<WebCore::SerializedScriptValue>&& sessionStateObject, bool wasCreatedByJSWithoutUserInteraction, bool wasRestoredFromSession, const std::optional<WebCore::PolicyContainer>&,
#if PLATFORM(IOS_FAMILY)
        WebCore::FloatRect exposedContentRect, WebCore::IntRect unobscuredContentRect, WebCore::FloatSize minimumLayoutSizeInScrollViewCoordinates, WebCore::IntSize contentSize, bool scaleIsInitial, WebCore::FloatBoxExtent obscuredInsets,
#endif
        const Vector<Ref<FrameState>>& children, const Vector<AtomString>& documentState
    );

    Vector<AtomString> m_documentState;
};

struct BackForwardListState {
    Vector<Ref<FrameState>> items;
    std::optional<uint32_t> currentIndex;
};

struct SessionState {
    BackForwardListState backForwardListState;
    uint64_t renderTreeSize;
    URL provisionalURL;
    bool isAppInitiated { true };
};

} // namespace WebKit
