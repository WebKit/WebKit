/*
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef ScriptElement_h
#define ScriptElement_h

#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"

namespace WebCore {

class CachedScript;
class Element;
class ScriptElementData;
class ScriptSourceCode;

class ScriptElement {
public:
    ScriptElement() { }
    virtual ~ScriptElement() { }

    virtual String scriptContent() const = 0;

    virtual String sourceAttributeValue() const = 0;
    virtual String charsetAttributeValue() const = 0;
    virtual String typeAttributeValue() const = 0;
    virtual String languageAttributeValue() const = 0;
    virtual String forAttributeValue() const = 0;

    virtual bool dispatchBeforeLoadEvent(const String& sourceURL) = 0;
    virtual void dispatchLoadEvent() = 0;
    virtual void dispatchErrorEvent() = 0;

    // A charset for loading the script (may be overridden by HTTP headers or a BOM).
    virtual String scriptCharset() const = 0;

    virtual bool shouldExecuteAsJavaScript() const = 0;

protected:
    // Helper functions used by our parent classes.
    static void insertedIntoDocument(ScriptElementData&, const String& sourceUrl);
    static void removedFromDocument(ScriptElementData&);
    static void childrenChanged(ScriptElementData&);
    static void finishParsingChildren(ScriptElementData&, const String& sourceUrl);
    static void handleSourceAttribute(ScriptElementData&, const String& sourceUrl);
};

// HTML/SVGScriptElement hold this struct as member variable
// and pass it to the static helper functions in ScriptElement
class ScriptElementData : private CachedResourceClient {
public:
    ScriptElementData(ScriptElement*, Element*);
    virtual ~ScriptElementData();

    bool ignoresLoadRequest() const;
    bool shouldExecuteAsJavaScript() const;

    String scriptContent() const;
    String scriptCharset() const;

    Element* element() const { return m_element; }
    bool createdByParser() const { return m_createdByParser; }
    void setCreatedByParser(bool value) { m_createdByParser = value; }
    bool haveFiredLoadEvent() const { return m_firedLoad; }
    void setHaveFiredLoadEvent(bool firedLoad) { m_firedLoad = firedLoad; }

    void requestScript(const String& sourceUrl);
    void evaluateScript(const ScriptSourceCode&);
    void stopLoadRequest();

    void execute(CachedScript*);

private:
    virtual void notifyFinished(CachedResource*);

private:
    ScriptElement* m_scriptElement;
    Element* m_element;
    CachedResourceHandle<CachedScript> m_cachedScript;
    bool m_createdByParser;
    bool m_requested;
    bool m_evaluated;
    bool m_firedLoad;
};

ScriptElement* toScriptElement(Element*);

}

#endif
