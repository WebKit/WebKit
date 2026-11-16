/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC) && HAVE(WK_SECURE_CODING_DATA_DETECTORS)

#include "WebHitTestResultData.h"

#include <wtf/ArgumentCoder.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

OBJC_CLASS DDScannerResult;

namespace WebKit {

struct CoreIPCDDSecureActionContextData {
    WebCore::IntRect highlightFrame;
    WebCore::IntRect aimFrame;
    RetainPtr<NSString> eventTitle;
    RetainPtr<NSString> leadingText;
    RetainPtr<NSString> trailingText;
    RetainPtr<NSString> coreSpotlightUniqueIdentifier;
    RetainPtr<NSDate> referenceDate;
    RetainPtr<NSString> hostUUID;
    RetainPtr<NSString> authorABUUID;
    RetainPtr<NSString> authorEmailAddress;
    RetainPtr<NSString> authorName;
    RetainPtr<NSURL> url;
    RetainPtr<NSString> matchedString;

    std::optional<Vector<RetainPtr<DDScannerResult>>> allResults;
    Vector<RetainPtr<DDScannerResult>> groupAllResults;
    RetainPtr<NSNumber> groupCategory;
    RetainPtr<NSString> groupTranscript;
    RetainPtr<NSString> selectionString;

    RetainPtr<DDScannerResult> mainResult;

    bool immediate;
    bool isRightClick;
    std::optional<bool> bypassScreentimeContactShield;
    RetainPtr<NSPersonNameComponents> authorNameComponents;
};

class CoreIPCDDSecureActionContext {
    WTF_MAKE_TZONE_ALLOCATED(CoreIPCDDSecureActionContext);
public:
    CoreIPCDDSecureActionContext(DDSecureActionContext *);
    CoreIPCDDSecureActionContext(CoreIPCDDSecureActionContextData&&);
    RetainPtr<id> toID() const;

private:
    friend struct IPC::ArgumentCoder<CoreIPCDDSecureActionContext>;
    CoreIPCDDSecureActionContextData m_data;
};


} // namespace WebKit

#endif // PLATFORM(MAC) && HAVE(WK_SECURE_CODING_DATA_DETECTORS)
