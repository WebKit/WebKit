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

#include "ISOProtectionSystemSpecificHeaderBox.h"
#include <wtf/Optional.h>

namespace WebCore {

class WEBCORE_EXPORT ISOFairPlayStreamingInfoBox final : public ISOFullBox {
public:
    static FourCC boxTypeName() { return "fpsi"; }

    FourCC scheme() const { return m_scheme; }

private:
    bool parse(JSC::DataView&, unsigned& offset) override;

    FourCC m_scheme;
};

class WEBCORE_EXPORT ISOFairPlayStreamingKeyRequestInfoBox final : public ISOFullBox {
public:
    static FourCC boxTypeName() { return "fkri"; }

    using KeyID = Vector<uint8_t, 16>;
    const KeyID& keyID() const { return m_keyID; }

private:
    bool parse(JSC::DataView&, unsigned& offset) override;

    KeyID m_keyID;
};

class WEBCORE_EXPORT ISOFairPlayStreamingKeyAssetIdBox final : public ISOBox {
public:
    static FourCC boxTypeName() { return "fkai"; }
    const Vector<uint8_t> data() const { return m_data; }

private:
    bool parse(JSC::DataView&, unsigned& offset) override;

    Vector<uint8_t> m_data;
};

class WEBCORE_EXPORT ISOFairPlayStreamingKeyContextBox final : public ISOBox {
public:
    static FourCC boxTypeName() { return "fkcx"; }
    const Vector<uint8_t> data() const { return m_data; }

private:
    bool parse(JSC::DataView&, unsigned& offset) override;

    Vector<uint8_t> m_data;
};

class WEBCORE_EXPORT ISOFairPlayStreamingKeyVersionListBox final : public ISOBox {
public:
    static FourCC boxTypeName() { return "fkvl"; }
    const Vector<uint8_t> versions() const { return m_versions; }

private:
    bool parse(JSC::DataView&, unsigned& offset) override;

    Vector<uint8_t> m_versions;
};

class WEBCORE_EXPORT ISOFairPlayStreamingKeyRequestBox final : public ISOBox {
public:
    static FourCC boxTypeName() { return "fpsk"; }

    const ISOFairPlayStreamingKeyRequestInfoBox& requestInfo() const { return m_requestInfo; }
    const Optional<ISOFairPlayStreamingKeyAssetIdBox>& assetID() const { return m_assetID; }
    const Optional<ISOFairPlayStreamingKeyContextBox>& content() const { return m_context; }
    const Optional<ISOFairPlayStreamingKeyVersionListBox>& versionList() const { return m_versionList; }

private:
    bool parse(JSC::DataView&, unsigned& offset) override;

    ISOFairPlayStreamingKeyRequestInfoBox m_requestInfo;
    Optional<ISOFairPlayStreamingKeyAssetIdBox> m_assetID;
    Optional<ISOFairPlayStreamingKeyContextBox> m_context;
    Optional<ISOFairPlayStreamingKeyVersionListBox> m_versionList;
};

class WEBCORE_EXPORT ISOFairPlayStreamingInitDataBox final : public ISOBox {
public:
    static FourCC boxTypeName() { return "fpsd"; }

    const ISOFairPlayStreamingInfoBox& info() const { return m_info; }
    const Vector<ISOFairPlayStreamingKeyRequestBox>& requests() const { return m_requests; }

private:
    bool parse(JSC::DataView&, unsigned& offset) override;

    ISOFairPlayStreamingInfoBox m_info;
    Vector<ISOFairPlayStreamingKeyRequestBox> m_requests;
};

class WEBCORE_EXPORT ISOFairPlayStreamingPsshBox final : public ISOProtectionSystemSpecificHeaderBox {
public:
    static const Vector<uint8_t>& fairPlaySystemID();

    const ISOFairPlayStreamingInitDataBox& initDataBox() { return m_initDataBox; }

private:
    bool parse(JSC::DataView&, unsigned& offset) override;

    ISOFairPlayStreamingInitDataBox m_initDataBox;
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ISOFairPlayStreamingPsshBox) \
static bool isType(const WebCore::ISOProtectionSystemSpecificHeaderBox& psshBox) { return psshBox.systemID() == WebCore::ISOFairPlayStreamingPsshBox::fairPlaySystemID(); }
SPECIALIZE_TYPE_TRAITS_END()
