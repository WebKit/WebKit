/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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

#if ENABLE(REMOTE_INSPECTOR)

#ifndef RemoteAutomationTarget_h
#define RemoteAutomationTarget_h

#include "RemoteControllableTarget.h"
#include <wtf/text/WTFString.h>

namespace Inspector {

class FrontendChannel;

class JS_EXPORT_PRIVATE RemoteAutomationTarget : public RemoteControllableTarget {
public:
    virtual ~RemoteAutomationTarget() { }

    bool isPaired() const { return m_paired; }
    void setIsPaired(bool);

    virtual String name() const = 0;
    virtual RemoteControllableTarget::Type type() const override { return RemoteControllableTarget::Type::Automation; }
    virtual bool remoteControlAllowed() const override { return !m_paired; };

private:
    bool m_paired { false };
};

} // namespace Inspector

SPECIALIZE_TYPE_TRAITS_CONTROLLABLE_TARGET(Inspector::RemoteAutomationTarget, Automation)

#endif // RemoteAutomationTarget_h

#endif // ENABLE(REMOTE_INSPECTOR)
