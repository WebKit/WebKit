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

#include "NavigatorUABrandVersion.h"
#include <WebCore/IDLTypes.h>
#include <WebCore/JSDOMPromiseDeferred.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>

namespace WebCore {
struct NavigatorUABrandVersion;
struct UADataValues;
struct UALowEntropyJSON;
struct UserAgentStringData;

class NavigatorUAData : public RefCounted<NavigatorUAData> {
public:
    static Ref<NavigatorUAData> create();
    static Ref<NavigatorUAData> create(Ref<UserAgentStringData>&&);
    const Vector<NavigatorUABrandVersion>& brands() const;
    bool mobile() const;
    String platform() const;
    UALowEntropyJSON toJSON() const;

    using ValuesPromise = DOMPromiseDeferred<IDLDictionary<UADataValues>>;
    void getHighEntropyValues(const Vector<String>& hints, ValuesPromise&&) const;
    ~NavigatorUAData();

private:
    NavigatorUAData();
    NavigatorUAData(Ref<UserAgentStringData>&&);
    static String createArbitraryVersion();
    static String createArbitraryBrand();

    bool overrideFromUserAgentString { false };
    bool mobileOverride { false };
    inline static LazyNeverDestroyed<Vector<NavigatorUABrandVersion>> m_brands;

    String platformOverride;
};
}
