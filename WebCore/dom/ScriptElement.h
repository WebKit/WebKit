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
class ScriptElement;
class ScriptSourceCode;

class ScriptElementData : private CachedResourceClient {
public:
    ScriptElementData(ScriptElement*, Element*, bool isEvaluated);
    virtual ~ScriptElementData();

    bool ignoresLoadRequest() const;
    bool shouldExecuteAsJavaScript() const;

    String scriptContent() const;
    String scriptCharset() const;
    bool isAsynchronous() const;
    bool isDeferred() const;
    bool isEvaluated() const { return m_isEvaluated; }

    Element* element() const { return m_element; }
    bool createdByParser() const { return m_createdByParser; }
    void setCreatedByParser(bool value) { m_createdByParser = value; }
    bool haveFiredLoadEvent() const { return m_firedLoad; }
    void setHaveFiredLoadEvent(bool firedLoad) { m_firedLoad = firedLoad; }

    void requestScript(const String& sourceUrl);
    void evaluateScript(const ScriptSourceCode&);
    void executeScript(const ScriptSourceCode&);
    void stopLoadRequest();

    void execute(CachedScript*);

private:
    virtual void notifyFinished(CachedResource*);

private:
    ScriptElement* m_scriptElement;
    Element* m_element;
    CachedResourceHandle<CachedScript> m_cachedScript;
    bool m_createdByParser; // HTML5: "parser-inserted"
    bool m_requested;
    bool m_isEvaluated; // HTML5: "already started"
    bool m_firedLoad;
};

class ScriptElement {
public:
    ScriptElement(Element* element, bool createdByParser, bool isEvaluated)
        : m_data(this, element, isEvaluated)
    {
        m_data.setCreatedByParser(createdByParser);
    }
    virtual ~ScriptElement() { }

    // A charset for loading the script (may be overridden by HTTP headers or a BOM).
    String scriptCharset() const;
    String scriptContent() const;
    bool shouldExecuteAsJavaScript() const;
    void executeScript(const ScriptSourceCode&);

    // XML parser calls these
    virtual String sourceAttributeValue() const = 0;
    virtual void dispatchLoadEvent() = 0;
    virtual void dispatchErrorEvent() = 0;

protected:
    bool haveFiredLoadEvent() const { return m_data.haveFiredLoadEvent(); }
    void setHaveFiredLoadEvent(bool firedLoad) { return m_data.setHaveFiredLoadEvent(firedLoad); }
    bool createdByParser() const { return m_data.createdByParser(); }
    bool isEvaluated() const { return m_data.isEvaluated(); }

    // Helper functions used by our parent classes.
    void insertedIntoDocument(const String& sourceUrl);
    void removedFromDocument();
    void childrenChanged();
    void finishParsingChildren(const String& sourceUrl);
    void handleSourceAttribute(const String& sourceUrl);

private:
    virtual String charsetAttributeValue() const = 0;
    virtual String typeAttributeValue() const = 0;
    virtual String languageAttributeValue() const = 0;
    virtual String forAttributeValue() const = 0;
    virtual String eventAttributeValue() const = 0;
    virtual bool asyncAttributeValue() const = 0;
    virtual bool deferAttributeValue() const = 0;

    friend class ScriptElementData;

    ScriptElementData m_data;
};

ScriptElement* toScriptElement(Element*);

}

#endif
