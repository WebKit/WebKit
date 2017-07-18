/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "PluginReplacement.h"

namespace WebCore {

class DOMWrapperWorld;
class HTMLVideoElement;

class QuickTimePluginReplacement final : public PluginReplacement {
public:
    static void registerPluginReplacement(PluginReplacementRegistrar);

    virtual ~QuickTimePluginReplacement();

    unsigned long long movieSize() const;
    void postEvent(const String&);

    HTMLVideoElement* parentElement() { return m_mediaElement.get(); }

private:
    QuickTimePluginReplacement(HTMLPlugInElement&, const Vector<String>& paramNames, const Vector<String>& paramValues);
    static Ref<PluginReplacement> create(HTMLPlugInElement&, const Vector<String>& paramNames, const Vector<String>& paramValues);
    static bool supportsMimeType(const String&);
    static bool supportsFileExtension(const String&);
    static bool supportsURL(const URL&) { return true; }
    static bool isEnabledBySettings(const Settings&);

    bool installReplacement(ShadowRoot&) final;
    JSC::JSObject* scriptObject() final { return m_scriptObject; }

    bool willCreateRenderer() final { return m_mediaElement; }
    RenderPtr<RenderElement> createElementRenderer(HTMLPlugInElement&, RenderStyle&&, const RenderTreePosition&) final;

    bool ensureReplacementScriptInjected();
    DOMWrapperWorld& isolatedWorld();

    HTMLPlugInElement* m_parentElement;
    RefPtr<HTMLVideoElement> m_mediaElement;
    const Vector<String> m_names;
    const Vector<String> m_values;
    JSC::JSObject* m_scriptObject { nullptr }; // FIXME: Why is it safe to have this pointer here? What keeps it alive during GC?
};

}
