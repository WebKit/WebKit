/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(REMOTE_INSPECTOR)

#include <JavaScriptCore/RemoteInspectionTarget.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class Page;

class PageDebuggable final : public Inspector::RemoteInspectionTarget {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(PageDebuggable);
public:
    PageDebuggable(Page&);
    ~PageDebuggable() { }

    Inspector::RemoteControllableTarget::Type type() const override { return Inspector::RemoteControllableTarget::Type::Web; }

    String name() const override;
    String url() const override;
    bool hasLocalDebugger() const override;

    void connect(Inspector::FrontendChannel*, bool isAutomaticConnection = false) override;
    void disconnect(Inspector::FrontendChannel*) override;
    void dispatchMessageFromRemote(const String& message) override;
    void setIndicating(bool) override;

    String nameOverride() const { return m_nameOverride; }
    void setNameOverride(const String&);

private:
    Page& m_page;
    String m_nameOverride;
    bool m_forcedDeveloperExtrasEnabled;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CONTROLLABLE_TARGET(WebCore::PageDebuggable, Web);

#endif // ENABLE(REMOTE_INSPECTOR)
