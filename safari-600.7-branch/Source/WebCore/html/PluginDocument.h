/*
 * Copyright (C) 2006, 2008, 2009, 2013 Apple Inc. All rights reserved.
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

#ifndef PluginDocument_h
#define PluginDocument_h

#include "HTMLDocument.h"

namespace WebCore {

class HTMLPlugInElement;
class Widget;

class PluginDocument final : public HTMLDocument {
public:
    static PassRefPtr<PluginDocument> create(Frame* frame, const URL& url)
    {
        return adoptRef(new PluginDocument(frame, url));
    }

    void setPluginElement(PassRefPtr<HTMLPlugInElement>);

    Widget* pluginWidget();
    HTMLPlugInElement* pluginElement() { return m_pluginElement.get(); }

    void detachFromPluginElement();

    void cancelManualPluginLoad();

    bool shouldLoadPluginManually() { return m_shouldLoadPluginManually; }

private:
    PluginDocument(Frame*, const URL&);

    virtual PassRefPtr<DocumentParser> createParser() override;

    void setShouldLoadPluginManually(bool loadManually) { m_shouldLoadPluginManually = loadManually; }

    bool m_shouldLoadPluginManually;
    RefPtr<HTMLPlugInElement> m_pluginElement;
};

inline bool isPluginDocument(const Document& document) { return document.isPluginDocument(); }
void isPluginDocument(const PluginDocument&); // Catch unnecessary runtime check of type known at compile time.

DOCUMENT_TYPE_CASTS(PluginDocument)

}

#endif // PluginDocument_h
