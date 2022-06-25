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

namespace WebCore {

class ISOFairPlayStreamingInfoBox final : public ISOFullBox {
public:
    WEBCORE_EXPORT ISOFairPlayStreamingInfoBox();
    WEBCORE_EXPORT ISOFairPlayStreamingInfoBox(const ISOFairPlayStreamingInfoBox&);
    ISOFairPlayStreamingInfoBox(ISOFairPlayStreamingInfoBox&&);

    static FourCC boxTypeName() { return "fpsi"; }

    FourCC scheme() const { return m_scheme; }

private:
    bool parse(JSC::DataView&, unsigned& offset) final;

    FourCC m_scheme;
};

class ISOFairPlayStreamingKeyRequestInfoBox final : public ISOFullBox {
public:
    static FourCC boxTypeName() { return "fkri"; }

    using KeyID = Vector<uint8_t, 16>;
    const KeyID& keyID() const { return m_keyID; }

private:
    bool parse(JSC::DataView&, unsigned& offset) final;

    KeyID m_keyID;
};

class ISOFairPlayStreamingKeyAssetIdBox final : public ISOBox {
public:
    ISOFairPlayStreamingKeyAssetIdBox() = default;
    WEBCORE_EXPORT ISOFairPlayStreamingKeyAssetIdBox(const ISOFairPlayStreamingKeyAssetIdBox&);
    ISOFairPlayStreamingKeyAssetIdBox(ISOFairPlayStreamingKeyAssetIdBox&&) = default;
    WEBCORE_EXPORT ~ISOFairPlayStreamingKeyAssetIdBox();

    ISOFairPlayStreamingKeyAssetIdBox& operator=(const ISOFairPlayStreamingKeyAssetIdBox&) = default;
    ISOFairPlayStreamingKeyAssetIdBox& operator=(ISOFairPlayStreamingKeyAssetIdBox&&) = default;

    static FourCC boxTypeName() { return "fkai"; }
    const Vector<uint8_t> data() const { return m_data; }

private:
    bool parse(JSC::DataView&, unsigned& offset) final;

    Vector<uint8_t> m_data;
};

class ISOFairPlayStreamingKeyContextBox final : public ISOBox {
public:
    ISOFairPlayStreamingKeyContextBox() = default;
    WEBCORE_EXPORT ISOFairPlayStreamingKeyContextBox(const ISOFairPlayStreamingKeyContextBox&);
    ISOFairPlayStreamingKeyContextBox(ISOFairPlayStreamingKeyContextBox&&) = default;
    WEBCORE_EXPORT ~ISOFairPlayStreamingKeyContextBox();

    ISOFairPlayStreamingKeyContextBox& operator=(const ISOFairPlayStreamingKeyContextBox&) = default;
    ISOFairPlayStreamingKeyContextBox& operator=(ISOFairPlayStreamingKeyContextBox&&) = default;

    static FourCC boxTypeName() { return "fkcx"; }
    const Vector<uint8_t> data() const { return m_data; }

private:
    bool parse(JSC::DataView&, unsigned& offset) final;

    Vector<uint8_t> m_data;
};

class ISOFairPlayStreamingKeyVersionListBox final : public ISOBox {
public:
    ISOFairPlayStreamingKeyVersionListBox() = default;
    WEBCORE_EXPORT ISOFairPlayStreamingKeyVersionListBox(const ISOFairPlayStreamingKeyVersionListBox&);
    ISOFairPlayStreamingKeyVersionListBox(ISOFairPlayStreamingKeyVersionListBox&&) = default;
    WEBCORE_EXPORT ~ISOFairPlayStreamingKeyVersionListBox();

    ISOFairPlayStreamingKeyVersionListBox& operator=(const ISOFairPlayStreamingKeyVersionListBox&) = default;
    ISOFairPlayStreamingKeyVersionListBox& operator=(ISOFairPlayStreamingKeyVersionListBox&&) = default;

    static FourCC boxTypeName() { return "fkvl"; }
    const Vector<uint8_t> versions() const { return m_versions; }

private:
    bool parse(JSC::DataView&, unsigned& offset) final;

    Vector<uint8_t> m_versions;
};

class ISOFairPlayStreamingKeyRequestBox final : public ISOBox {
public:
    ISOFairPlayStreamingKeyRequestBox() = default;
    WEBCORE_EXPORT ISOFairPlayStreamingKeyRequestBox(const ISOFairPlayStreamingKeyRequestBox&);
    ISOFairPlayStreamingKeyRequestBox(ISOFairPlayStreamingKeyRequestBox&&) = default;
    WEBCORE_EXPORT ~ISOFairPlayStreamingKeyRequestBox();

    static FourCC boxTypeName() { return "fpsk"; }

    const ISOFairPlayStreamingKeyRequestInfoBox& requestInfo() const { return m_requestInfo; }
    const std::optional<ISOFairPlayStreamingKeyAssetIdBox>& assetID() const { return m_assetID; }
    const std::optional<ISOFairPlayStreamingKeyContextBox>& content() const { return m_context; }
    const std::optional<ISOFairPlayStreamingKeyVersionListBox>& versionList() const { return m_versionList; }

private:
    bool parse(JSC::DataView&, unsigned& offset) final;

    ISOFairPlayStreamingKeyRequestInfoBox m_requestInfo;
    std::optional<ISOFairPlayStreamingKeyAssetIdBox> m_assetID;
    std::optional<ISOFairPlayStreamingKeyContextBox> m_context;
    std::optional<ISOFairPlayStreamingKeyVersionListBox> m_versionList;
};

class ISOFairPlayStreamingInitDataBox final : public ISOBox {
public:
    WEBCORE_EXPORT ISOFairPlayStreamingInitDataBox();
    WEBCORE_EXPORT ~ISOFairPlayStreamingInitDataBox();

    static FourCC boxTypeName() { return "fpsd"; }

    const ISOFairPlayStreamingInfoBox& info() const { return m_info; }
    const Vector<ISOFairPlayStreamingKeyRequestBox>& requests() const { return m_requests; }

private:
    bool parse(JSC::DataView&, unsigned& offset) final;

    ISOFairPlayStreamingInfoBox m_info;
    Vector<ISOFairPlayStreamingKeyRequestBox> m_requests;
};

class ISOFairPlayStreamingPsshBox final : public ISOProtectionSystemSpecificHeaderBox {
public:
    WEBCORE_EXPORT ISOFairPlayStreamingPsshBox();
    WEBCORE_EXPORT ~ISOFairPlayStreamingPsshBox();

    static const Vector<uint8_t>& fairPlaySystemID();

    const ISOFairPlayStreamingInitDataBox& initDataBox() { return m_initDataBox; }

private:
    bool parse(JSC::DataView&, unsigned& offset) final;

    ISOFairPlayStreamingInitDataBox m_initDataBox;
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ISOFairPlayStreamingPsshBox) \
static bool isType(const WebCore::ISOProtectionSystemSpecificHeaderBox& psshBox) { return psshBox.systemID() == WebCore::ISOFairPlayStreamingPsshBox::fairPlaySystemID(); }
SPECIALIZE_TYPE_TRAITS_END()
