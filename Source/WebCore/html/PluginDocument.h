/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "HTMLDocument.h"

namespace WebCore {

class HTMLPlugInElement;
class Widget;

class PluginDocument final : public HTMLDocument {
    WTF_MAKE_ISO_ALLOCATED(PluginDocument);
public:
    static Ref<PluginDocument> create(Frame& frame, const URL& url)
    {
        return adoptRef(*new PluginDocument(frame, url));
    }

    WEBCORE_EXPORT Widget* pluginWidget();
    HTMLPlugInElement* pluginElement() { return m_pluginElement.get(); }

    void setPluginElement(HTMLPlugInElement&);
    void detachFromPluginElement();

    void cancelManualPluginLoad();

    bool shouldLoadPluginManually() const { return m_shouldLoadPluginManually; }

private:
    PluginDocument(Frame&, const URL&);

    Ref<DocumentParser> createParser() final;

    RefPtr<HTMLPlugInElement> m_pluginElement;
    bool m_shouldLoadPluginManually { true };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::PluginDocument)
    static bool isType(const WebCore::Document& document) { return document.isPluginDocument(); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::Document>(node) && isType(downcast<WebCore::Document>(node)); }
SPECIALIZE_TYPE_TRAITS_END()
