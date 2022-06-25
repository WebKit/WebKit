/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

OBJC_CLASS NSArray;
OBJC_CLASS NSDictionary;

#include "APIObject.h"
#include "InteractionInformationAtPosition.h"
#include <wtf/RetainPtr.h>

namespace API {

class ContextMenuElementInfo final : public ObjectImpl<Object::Type::ContextMenuElementInfo> {
public:
    template<typename... Args> static Ref<ContextMenuElementInfo> create(Args&&... args)
    {
        return adoptRef(*new ContextMenuElementInfo(std::forward<Args>(args)...));
    }
    
    const WTF::URL& url() const { return m_interactionInformation.url; }

    const WebKit::InteractionInformationAtPosition& interactionInformation() const { return m_interactionInformation; }

    const RetainPtr<NSDictionary> userInfo() const { return m_userInfo; }

private:
    ContextMenuElementInfo(const WebKit::InteractionInformationAtPosition&);
    ContextMenuElementInfo(const WebKit::InteractionInformationAtPosition&, NSDictionary *);
    
    WebKit::InteractionInformationAtPosition m_interactionInformation;
    RetainPtr<NSDictionary> m_userInfo;
};

} // namespace API

#endif
