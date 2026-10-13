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

#include "APIObject.h"
#include <wtf/text/WTFString.h>

namespace WebKit {
enum class ContentWorldOption : uint8_t;
}

namespace API {

class ContentWorldConfiguration : public ObjectImpl<Object::Type::ContentWorldConfiguration> {
public:
    static Ref<ContentWorldConfiguration> create();

    explicit ContentWorldConfiguration();
    virtual ~ContentWorldConfiguration();

    Ref<ContentWorldConfiguration> copy() const;

    const WTF::String& LEGACY_NODELETE name() const LIFETIME_BOUND;
    void setName(WTF::String&&);

    bool LEGACY_NODELETE allowAccessToClosedShadowRoots() const;
    void LEGACY_NODELETE setAllowAccessToClosedShadowRoots(bool);

    bool LEGACY_NODELETE allowAutofill() const;
    void LEGACY_NODELETE setAllowAutofill(bool);

    bool LEGACY_NODELETE allowElementUserInfo() const;
    void LEGACY_NODELETE setAllowElementUserInfo(bool);

    bool LEGACY_NODELETE disableLegacyBuiltinOverrides() const;
    void LEGACY_NODELETE setDisableLegacyBuiltinOverrides(bool);

    bool LEGACY_NODELETE allowJSHandleCreation() const;
    void LEGACY_NODELETE setAllowJSHandleCreation(bool);

    bool LEGACY_NODELETE allowNodeSnapshotCreation() const;
    void LEGACY_NODELETE setAllowNodeSnapshotCreation(bool);

    bool LEGACY_NODELETE isInspectable() const;
    void LEGACY_NODELETE setInspectable(bool);

    OptionSet<WebKit::ContentWorldOption> LEGACY_NODELETE optionSet() const;

private:
    struct Data {
        Data();

        WTF::String name;
        bool allowAccessToClosedShadowRoots : 1 { false };
        bool allowAutofill : 1 { false };
        bool allowElementUserInfo : 1 { false };
        bool disableLegacyBuiltinOverrides : 1 { false };
        bool allowJSHandleCreation : 1 { false };
        bool allowNodeSnapshotCreation : 1 { false };
        bool inspectable : 1 { true };
    };

    // All data members should be added to the Data structure to avoid breaking ContentWorldConfiguration::copy().
    Data m_data;
};

} // namespace API

SPECIALIZE_TYPE_TRAITS_API_OBJECT(ContentWorldConfiguration);
