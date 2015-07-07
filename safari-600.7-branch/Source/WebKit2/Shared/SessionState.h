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

#ifndef SessionState_h
#define SessionState_h

#if PLATFORM(COCOA)
#include "ViewSnapshotStore.h"
#endif

#include <WebCore/FloatRect.h>
#include <WebCore/IntRect.h>
#include <WebCore/URL.h>
#include <wtf/Optional.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class ArgumentDecoder;
class ArgumentEncoder;
}

namespace WebKit {

struct HTTPBody {
    struct Element {
        void encode(IPC::ArgumentEncoder&) const;
        static bool decode(IPC::ArgumentDecoder&, Element&);

        enum class Type {
            Data,
            File,
            Blob,
        };

        Type type = Type::Data;

        // Data.
        Vector<char> data;

        // File.
        String filePath;
        int64_t fileStart;
        Optional<int64_t> fileLength;
        Optional<double> expectedFileModificationTime;

        // Blob.
        String blobURLString;
    };

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, HTTPBody&);

    String contentType;
    Vector<Element> elements;
};

struct FrameState {
    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, FrameState&);

    String urlString;
    String originalURLString;
    String referrer;
    String target;

    Vector<String> documentState;
    Optional<Vector<uint8_t>> stateObjectData;

    int64_t documentSequenceNumber;
    int64_t itemSequenceNumber;

    WebCore::IntPoint scrollPoint;
    float pageScaleFactor;

    Optional<HTTPBody> httpBody;

    // FIXME: These should not be per frame.
#if PLATFORM(IOS)
    WebCore::FloatRect exposedContentRect;
    WebCore::IntRect unobscuredContentRect;
    WebCore::FloatSize minimumLayoutSizeInScrollViewCoordinates;
    WebCore::IntSize contentSize;
    bool scaleIsInitial = false;
#endif

    Vector<FrameState> children;
};

struct PageState {
    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, PageState&);

    String title;
    FrameState mainFrameState;
};

struct BackForwardListItemState {
    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, BackForwardListItemState&);

    uint64_t identifier;

    PageState pageState;
#if PLATFORM(COCOA)
    RefPtr<ViewSnapshot> snapshot;
#endif

};

struct BackForwardListState {
    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, BackForwardListState&);

    Vector<BackForwardListItemState> items;
    Optional<uint32_t> currentIndex;
};

struct SessionState {
    BackForwardListState backForwardListState;
    WebCore::URL provisionalURL;
};

} // namespace WebKit

#endif // SessionState_h
