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

class ScriptElement : private CachedResourceClient {
public:
    ScriptElement(Element*, bool createdByParser, bool isEvaluated);
    virtual ~ScriptElement();
    
    Element* element() const { return m_element; }

    // A charset for loading the script (may be overridden by HTTP headers or a BOM).
    String scriptCharset() const;

    String scriptContent() const;
    bool shouldExecuteAsJavaScript() const;
    void executeScript(const ScriptSourceCode&);
    void execute(CachedScript*);

    // XML parser calls these
    virtual String sourceAttributeValue() const = 0;
    virtual void dispatchLoadEvent() = 0;
    virtual void dispatchErrorEvent() = 0;

protected:
    bool haveFiredLoadEvent() const { return m_firedLoad; }
    void setHaveFiredLoadEvent(bool firedLoad) { m_firedLoad = firedLoad; }
    bool createdByParser() const { return m_createdByParser; }
    bool isEvaluated() const { return m_isEvaluated; }

    // Helper functions used by our parent classes.
    void insertedIntoDocument(const String& sourceUrl);
    void removedFromDocument();
    void childrenChanged();
    void finishParsingChildren(const String& sourceUrl);
    void handleSourceAttribute(const String& sourceUrl);

private:
    bool ignoresLoadRequest() const;
    bool isAsynchronous() const;
    bool isDeferred() const;

    void requestScript(const String& sourceUrl);
    void evaluateScript(const ScriptSourceCode&);
    void stopLoadRequest();

    virtual void notifyFinished(CachedResource*);

    virtual String charsetAttributeValue() const = 0;
    virtual String typeAttributeValue() const = 0;
    virtual String languageAttributeValue() const = 0;
    virtual String forAttributeValue() const = 0;
    virtual String eventAttributeValue() const = 0;
    virtual bool asyncAttributeValue() const = 0;
    virtual bool deferAttributeValue() const = 0;

    Element* m_element;
    CachedResourceHandle<CachedScript> m_cachedScript;
    bool m_createdByParser; // HTML5: "parser-inserted"
    bool m_requested;
    bool m_isEvaluated; // HTML5: "already started"
    bool m_firedLoad;
};

ScriptElement* toScriptElement(Element*);

}

#endif
