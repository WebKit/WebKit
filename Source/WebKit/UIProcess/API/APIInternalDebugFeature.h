/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "APIFeatureStatus.h"
#include "APIObject.h"
#include <wtf/text/WTFString.h>

namespace API {

class InternalDebugFeature final : public ObjectImpl<Object::Type::InternalDebugFeature> {
public:
    static Ref<InternalDebugFeature> create(const WTF::String& name, const WTF::String& key, FeatureStatus, const WTF::String& details, bool defaultValue, bool hidden);
    virtual ~InternalDebugFeature() = default;

    WTF::String name() const { return m_name; }
    WTF::String key() const { return m_key; }
    FeatureStatus status() const { return m_status; }
    WTF::String details() const { return m_details; }
    bool defaultValue() const { return m_defaultValue; }
    bool isHidden() const { return m_hidden; }

private:
    explicit InternalDebugFeature(const WTF::String& name, const WTF::String& key, FeatureStatus, const WTF::String& details, bool defaultValue, bool hidden);

    WTF::String m_name;
    WTF::String m_key;
    FeatureStatus m_status;
    WTF::String m_details;
    bool m_defaultValue;
    bool m_hidden;
};

}
