/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)
#include "CoreIPCTypes.h"

#if USE(AVFOUNDATION)
OBJC_CLASS AVOutputContext;
#endif
OBJC_CLASS NSSomeFoundationType;
#if ENABLE(DATA_DETECTION)
OBJC_CLASS DDScannerResult;
#endif

namespace WebKit {

#if USE(AVFOUNDATION)
class CoreIPCAVOutputContext {
public:
    CoreIPCAVOutputContext(AVOutputContext *);
    CoreIPCAVOutputContext(const RetainPtr<AVOutputContext>& object)
        : CoreIPCAVOutputContext(object.get())
    {
    }

    static bool isValidDictionary(CoreIPCDictionary&);
    RetainPtr<id> toID() const;

private:
    friend struct IPC::ArgumentCoder<CoreIPCAVOutputContext, void>;

    CoreIPCAVOutputContext(CoreIPCDictionary&& propertyList)
        : m_propertyList(WTFMove(propertyList))
    {
    }

    CoreIPCDictionary m_propertyList;
};
#endif
class CoreIPCNSSomeFoundationType {
public:
    CoreIPCNSSomeFoundationType(NSSomeFoundationType *);
    CoreIPCNSSomeFoundationType(const RetainPtr<NSSomeFoundationType>& object)
        : CoreIPCNSSomeFoundationType(object.get())
    {
    }

    static bool isValidDictionary(CoreIPCDictionary&);
    RetainPtr<id> toID() const;

private:
    friend struct IPC::ArgumentCoder<CoreIPCNSSomeFoundationType, void>;

    CoreIPCNSSomeFoundationType(CoreIPCDictionary&& propertyList)
        : m_propertyList(WTFMove(propertyList))
    {
    }

    CoreIPCDictionary m_propertyList;
};
#if ENABLE(DATA_DETECTION)
class CoreIPCDDScannerResult {
public:
    CoreIPCDDScannerResult(DDScannerResult *);
    CoreIPCDDScannerResult(const RetainPtr<DDScannerResult>& object)
        : CoreIPCDDScannerResult(object.get())
    {
    }

    static bool isValidDictionary(CoreIPCDictionary&);
    RetainPtr<id> toID() const;

private:
    friend struct IPC::ArgumentCoder<CoreIPCDDScannerResult, void>;

    CoreIPCDDScannerResult(CoreIPCDictionary&& propertyList)
        : m_propertyList(WTFMove(propertyList))
    {
    }

    CoreIPCDictionary m_propertyList;
};
#endif

} // namespace WebKit

#endif // PLATFORM(COCOA)
