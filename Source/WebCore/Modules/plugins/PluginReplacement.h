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

#include "RenderPtr.h"
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class JSObject;
}

namespace WebCore {

class HTMLPlugInElement;
class RenderElement;
class RenderStyle;
class RenderTreePosition;
class Settings;
class ShadowRoot;

class PluginReplacement : public RefCounted<PluginReplacement> {
public:
    virtual ~PluginReplacement() = default;

    struct InstallResult {
        bool success;
#if PLATFORM(COCOA)
        JSC::JSValue scriptObject { };
#endif
    };

    virtual InstallResult installReplacement(ShadowRoot&) = 0;

    virtual bool willCreateRenderer() { return false; }
    virtual RenderPtr<RenderElement> createElementRenderer(HTMLPlugInElement&, RenderStyle&&, const RenderTreePosition&) = 0;
};

typedef Ref<PluginReplacement> (*CreatePluginReplacement)(HTMLPlugInElement&, const Vector<String>& paramNames, const Vector<String>& paramValues);
typedef bool (*PluginReplacementSupportsType)(const String&);
typedef bool (*PluginReplacementSupportsFileExtension)(const String&);
typedef bool (*PluginReplacementSupportsURL)(const URL&);
typedef bool (*PluginReplacementEnabledForSettings)(const Settings&);

class ReplacementPlugin {
public:
    ReplacementPlugin(CreatePluginReplacement constructor, PluginReplacementSupportsType supportsType, PluginReplacementSupportsFileExtension supportsFileExtension, PluginReplacementSupportsURL supportsURL, PluginReplacementEnabledForSettings isEnabledBySettings)
        : m_constructor(constructor)
        , m_supportsType(supportsType)
        , m_supportsFileExtension(supportsFileExtension)
        , m_supportsURL(supportsURL)
        , m_isEnabledBySettings(isEnabledBySettings)
    {
    }

    explicit ReplacementPlugin(const ReplacementPlugin& other)
        : m_constructor(other.m_constructor)
        , m_supportsType(other.m_supportsType)
        , m_supportsFileExtension(other.m_supportsFileExtension)
        , m_supportsURL(other.m_supportsURL)
        , m_isEnabledBySettings(other.m_isEnabledBySettings)
    {
    }

    Ref<PluginReplacement> create(HTMLPlugInElement& element, const Vector<String>& paramNames, const Vector<String>& paramValues) const { return m_constructor(element, paramNames, paramValues); }
    bool supportsType(const String& mimeType) const { return m_supportsType(mimeType); }
    bool supportsFileExtension(const String& extension) const { return m_supportsFileExtension(extension); }
    bool supportsURL(const URL& url) const { return m_supportsURL(url); }
    bool isEnabledBySettings(const Settings& settings) const { return m_isEnabledBySettings(settings); };

private:
    CreatePluginReplacement m_constructor;
    PluginReplacementSupportsType m_supportsType;
    PluginReplacementSupportsFileExtension m_supportsFileExtension;
    PluginReplacementSupportsURL m_supportsURL;
    PluginReplacementEnabledForSettings m_isEnabledBySettings;
};

typedef void (*PluginReplacementRegistrar)(const ReplacementPlugin&);

}
