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

#include "APIObject.h"
#include "DebuggableInfoData.h"
#include <WebCore/InspectorDebuggableType.h>

namespace API {

class DebuggableInfo final : public ObjectImpl<Object::Type::DebuggableInfo> {
public:
    static Ref<DebuggableInfo> create(const WebKit::DebuggableInfoData&);
    DebuggableInfo() = default;
    virtual ~DebuggableInfo();

    Inspector::DebuggableType debuggableType() const { return m_data.debuggableType; }
    void setDebuggableType(Inspector::DebuggableType debuggableType) { m_data.debuggableType = debuggableType; }

    const WTF::String& targetPlatformName() const { return m_data.targetPlatformName; }
    void setTargetPlatformName(const WTF::String& targetPlatformName) { m_data.targetPlatformName = targetPlatformName; }

    const WTF::String& targetBuildVersion() const { return m_data.targetBuildVersion; }
    void setTargetBuildVersion(const WTF::String& targetBuildVersion) { m_data.targetBuildVersion = targetBuildVersion; }

    const WTF::String& targetProductVersion() const { return m_data.targetProductVersion; }
    void setTargetProductVersion(const WTF::String& targetProductVersion) { m_data.targetProductVersion = targetProductVersion; }

    bool targetIsSimulator() const { return m_data.targetIsSimulator; }
    void setTargetIsSimulator(bool targetIsSimulator) { m_data.targetIsSimulator = targetIsSimulator; }

    const WebKit::DebuggableInfoData& debuggableInfoData() const { return m_data; }

private:
    explicit DebuggableInfo(const WebKit::DebuggableInfoData&);

    WebKit::DebuggableInfoData m_data;
};

} // namespace API
