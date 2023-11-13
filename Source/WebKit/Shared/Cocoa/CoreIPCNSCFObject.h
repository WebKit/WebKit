/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#include "CoreIPCArray.h"
#include "CoreIPCCFType.h"
#include "CoreIPCColor.h"
#include "CoreIPCData.h"
#include "CoreIPCDate.h"
#include "CoreIPCDictionary.h"
#include "CoreIPCFont.h"
#include "CoreIPCNumber.h"
#include "CoreIPCSecureCoding.h"
#include "CoreIPCString.h"
#include "CoreIPCURL.h"
#include <wtf/RetainPtr.h>

namespace WebKit {

class CoreIPCNSCFObject {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using ObjectValue = std::variant<
        std::nullptr_t,
        CoreIPCArray,
        CoreIPCCFType,
        CoreIPCColor,
        CoreIPCData,
        CoreIPCDate,
        CoreIPCDictionary,
        CoreIPCFont,
        CoreIPCNumber,
        CoreIPCSecureCoding,
        CoreIPCString,
        CoreIPCURL
    >;

    CoreIPCNSCFObject(id);

    RetainPtr<id> toID() const;

    static bool valueIsAllowed(IPC::Decoder&, ObjectValue&);

private:
    friend struct IPC::ArgumentCoder<CoreIPCNSCFObject, void>;

    CoreIPCNSCFObject(ObjectValue&& value)
        : m_value(WTFMove(value))
    {
    }

    ObjectValue m_value;
};

} // namespace WebKit

#endif // PLATFORM(COCOA)
