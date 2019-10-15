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
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/IntRect.h>
#include <WebCore/SerializedScriptValue.h>
#include <wtf/Optional.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

bool isValidEnum(WebCore::ShouldOpenExternalURLsPolicy);

struct HTTPBody {
    struct Element {
        void encode(IPC::Encoder&) const;
        static Optional<Element> decode(IPC::Decoder&);

        enum class Type {
            Data,
            File,
            Blob,
        };

        // FIXME: This should be a Variant. It's also unclear why we don't just use FormDataElement here.
        Type type = Type::Data;

        // Data.
        Vector<char> data;

        // File.
        String filePath;
        int64_t fileStart;
        Optional<int64_t> fileLength;
        Optional<WallTime> expectedFileModificationTime;

        // Blob.
        String blobURLString;
    };

    void encode(IPC::Encoder&) const;
    static bool decode(IPC::Decoder&, HTTPBody&);

    String contentType;
    Vector<Element> elements;
};

struct FrameState {
    void encode(IPC::Encoder&) const;
    static Optional<FrameState> decode(IPC::Decoder&);

    String urlString;
    String originalURLString;
    String referrer;
    String target;

    Vector<String> documentState;
    Optional<Vector<uint8_t>> stateObjectData;

    int64_t documentSequenceNumber { 0 };
    int64_t itemSequenceNumber { 0 };

    WebCore::IntPoint scrollPosition;
    bool shouldRestoreScrollPosition { true };
    float pageScaleFactor { 0 };

    Optional<HTTPBody> httpBody;

    // FIXME: These should not be per frame.
#if PLATFORM(IOS_FAMILY)
    WebCore::FloatRect exposedContentRect;
    WebCore::IntRect unobscuredContentRect;
    WebCore::FloatSize minimumLayoutSizeInScrollViewCoordinates;
    WebCore::IntSize contentSize;
    bool scaleIsInitial { false };
    WebCore::FloatBoxExtent obscuredInsets;
#endif

    Vector<FrameState> children;

    // This is only used to help debug <rdar://problem/48634553>.
    bool isDestructed { false };
    ~FrameState() { isDestructed = true; }
};

struct PageState {
    void encode(IPC::Encoder&) const;
    static bool decode(IPC::Decoder&, PageState&);

    String title;
    FrameState mainFrameState;
    WebCore::ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy { WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow };
    RefPtr<WebCore::SerializedScriptValue> sessionStateObject;
};

struct BackForwardListItemState {
    void encode(IPC::Encoder&) const;
    static Optional<BackForwardListItemState> decode(IPC::Decoder&);

    WebCore::BackForwardItemIdentifier identifier;

    PageState pageState;
#if PLATFORM(COCOA) || PLATFORM(GTK)
    RefPtr<ViewSnapshot> snapshot;
#endif
    bool hasCachedPage { false };
};

struct BackForwardListState {
    void encode(IPC::Encoder&) const;
    static Optional<BackForwardListState> decode(IPC::Decoder&);

    Vector<BackForwardListItemState> items;
    Optional<uint32_t> currentIndex;
};

struct SessionState {
    BackForwardListState backForwardListState;
    uint64_t renderTreeSize;
    URL provisionalURL;
};

} // namespace WebKit
