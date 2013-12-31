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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PluginReplacement_h
#define PluginReplacement_h

#include "RenderPtr.h"
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class JSObject;
}

namespace WebCore {

class HTMLPlugInElement;
class RenderElement;
class RenderStyle;
class ShadowRoot;

class PluginReplacement : public RefCounted<PluginReplacement> {
public:
    virtual ~PluginReplacement() { }

    virtual bool installReplacement(ShadowRoot*) = 0;
    virtual JSC::JSObject* scriptObject() { return 0; }

    virtual bool willCreateRenderer() { return false; }
    virtual RenderPtr<RenderElement> createElementRenderer(HTMLPlugInElement&, PassRef<RenderStyle>) = 0;

protected:
    PluginReplacement() { }
};

typedef PassRefPtr<PluginReplacement> (*CreatePluginReplacement)(HTMLPlugInElement&, const Vector<String>& paramNames, const Vector<String>& paramValues);
typedef bool (*PluginReplacementSupportsType)(const String&);
typedef bool (*PluginReplacementSupportsFileExtension)(const String&);

class ReplacementPlugin {
public:
    ReplacementPlugin(CreatePluginReplacement constructor, PluginReplacementSupportsType supportsType, PluginReplacementSupportsFileExtension supportsFileExtension)
        : m_constructor(constructor)
        , m_supportsType(supportsType)
        , m_supportsFileExtension(supportsFileExtension)
    {
    }

    explicit ReplacementPlugin(const ReplacementPlugin& other)
        : m_constructor(other.m_constructor)
        , m_supportsType(other.m_supportsType)
        , m_supportsFileExtension(other.m_supportsFileExtension)
    {
    }

    PassRefPtr<PluginReplacement> create(HTMLPlugInElement& element, const Vector<String>& paramNames, const Vector<String>& paramValues) const { return m_constructor(element, paramNames, paramValues); }
    bool supportsType(const String& mimeType) const { return m_supportsType(mimeType); }
    bool supportsFileExtension(const String& extension) const { return m_supportsFileExtension(extension); }

private:
    CreatePluginReplacement m_constructor;
    PluginReplacementSupportsType m_supportsType;
    PluginReplacementSupportsFileExtension m_supportsFileExtension;
};

typedef void (*PluginReplacementRegistrar)(const ReplacementPlugin&);

}

#endif
