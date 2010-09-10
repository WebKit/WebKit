/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include "ScriptElement.h"

#include "AsyncScriptRunner.h"
#include "CachedScript.h"
#include "CachedResourceLoader.h"
#include "Document.h"
#include "DocumentParser.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLNames.h"
#include "HTMLScriptElement.h"
#include "MIMETypeRegistry.h"
#include "Page.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "Settings.h"
#include "Text.h"
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringHash.h>

#if ENABLE(SVG)
#include "SVGNames.h"
#include "SVGScriptElement.h"
#endif

namespace WebCore {

void ScriptElement::insertedIntoDocument(ScriptElementData& data, const String& sourceUrl)
{
    if (data.createdByParser() && !data.isAsynchronous())
        return;

    // http://www.whatwg.org/specs/web-apps/current-work/#script

    // If the element's Document has an active parser, and the parser's script
    // nesting level is non-zero, but this script element does not have the
    // "parser-inserted" flag set, the user agent must set the element's
    // "write-neutralised" flag.
    DocumentParser* parser = data.element()->document()->parser();
    if (parser && parser->hasInsertionPoint())
        data.setWriteDisabled(true);

    if (!sourceUrl.isEmpty()) {
        data.requestScript(sourceUrl);
        return;
    }

    // If there's an empty script node, we shouldn't evaluate the script
    // because if a script is inserted afterwards (by setting text or innerText)
    // it should be evaluated, and evaluateScript only evaluates a script once.
    data.evaluateScript(ScriptSourceCode(data.scriptContent(), data.element()->document()->url())); // FIXME: Provide a real starting line number here.
}

void ScriptElement::removedFromDocument(ScriptElementData& data)
{
    // Eventually stop loading any not-yet-finished content
    data.stopLoadRequest();
}

void ScriptElement::childrenChanged(ScriptElementData& data)
{
    if (data.createdByParser())
        return;

    Element* element = data.element();

    // If a node is inserted as a child of the script element
    // and the script element has been inserted in the document
    // we evaluate the script.
    if (element->inDocument() && element->firstChild())
        data.evaluateScript(ScriptSourceCode(data.scriptContent(), element->document()->url())); // FIXME: Provide a real starting line number here
}

void ScriptElement::finishParsingChildren(ScriptElementData& data, const String& sourceUrl)
{
    // The parser just reached </script>. If we have no src and no text,
    // allow dynamic loading later.
    if (sourceUrl.isEmpty() && data.scriptContent().isEmpty())
        data.setCreatedByParser(false);
}

void ScriptElement::handleSourceAttribute(ScriptElementData& data, const String& sourceUrl)
{
    if (data.ignoresLoadRequest() || sourceUrl.isEmpty())
        return;

    data.requestScript(sourceUrl);
}

// Helper function
static bool isSupportedJavaScriptLanguage(const String& language)
{
    typedef HashSet<String, CaseFoldingHash> LanguageSet;
    DEFINE_STATIC_LOCAL(LanguageSet, languages, ());
    if (languages.isEmpty()) {
        languages.add("javascript");
        languages.add("javascript");
        languages.add("javascript1.0");
        languages.add("javascript1.1");
        languages.add("javascript1.2");
        languages.add("javascript1.3");
        languages.add("javascript1.4");
        languages.add("javascript1.5");
        languages.add("javascript1.6");
        languages.add("javascript1.7");
        languages.add("livescript");
        languages.add("ecmascript");
        languages.add("jscript");                
    }

    return languages.contains(language);
}

// ScriptElementData
ScriptElementData::ScriptElementData(ScriptElement* scriptElement, Element* element)
    : m_scriptElement(scriptElement)
    , m_element(element)
    , m_cachedScript(0)
    , m_createdByParser(false)
    , m_writeDisabled(false)
    , m_requested(false)
    , m_evaluated(false)
    , m_firedLoad(false)
{
    ASSERT(m_scriptElement);
    ASSERT(m_element);
}

ScriptElementData::~ScriptElementData()
{
    stopLoadRequest();
}

void ScriptElementData::requestScript(const String& sourceUrl)
{
    Document* document = m_element->document();

    // FIXME: Eventually we'd like to evaluate scripts which are inserted into a 
    // viewless document but this'll do for now.
    // See http://bugs.webkit.org/show_bug.cgi?id=5727
    if (!document->frame())
        return;

    if (!m_element->dispatchBeforeLoadEvent(sourceUrl))
        return;

    ASSERT(!m_cachedScript);
    m_cachedScript = document->cachedResourceLoader()->requestScript(sourceUrl, scriptCharset());
    m_requested = true;

    // m_createdByParser is never reset - always resied at the initial value set while parsing.
    // m_evaluated is left untouched as well to avoid script reexecution, if a <script> element
    // is removed and reappended to the document.
    m_firedLoad = false;

    if (m_cachedScript) {
        m_cachedScript->addClient(this);
        return;
    }

    m_scriptElement->dispatchErrorEvent();
}

void ScriptElementData::evaluateScript(const ScriptSourceCode& sourceCode)
{
    if (m_evaluated || sourceCode.isEmpty() || !shouldExecuteAsJavaScript())
        return;

    if (Frame* frame = m_element->document()->frame()) {
        if (!frame->script()->canExecuteScripts(AboutToExecuteScript))
            return;

        m_evaluated = true;

        // http://www.whatwg.org/specs/web-apps/current-work/#script

        // If the script element's "write-neutralised" flag is set, then flag
        // the Document the script element was in when the "write-neutralised"
        // flag was set as being itself "write-neutralised". Let neutralised doc
        // be that Document.
        if (m_writeDisabled) {
            ASSERT(!m_element->document()->writeDisabled());
            m_element->document()->setWriteDisabled(true);
        }

        // Create a script from the script element node, using the script
        // block's source and the script block's type.
        // Note: This is where the script is compiled and actually executed.
        frame->script()->evaluate(sourceCode);

        // Remove the "write-neutralised" flag from neutralised doc, if it was
        // set in the earlier step.
        if (m_writeDisabled) {
            ASSERT(m_element->document()->writeDisabled());
            m_element->document()->setWriteDisabled(false);
        }

        Document::updateStyleForAllDocuments();
    }
}

void ScriptElementData::stopLoadRequest()
{
    if (m_cachedScript) {
        m_cachedScript->removeClient(this);
        m_cachedScript = 0;
    }
}

void ScriptElementData::execute(CachedScript* cachedScript)
{
    ASSERT(cachedScript);
    if (cachedScript->errorOccurred())
        m_scriptElement->dispatchErrorEvent();
    else {
        evaluateScript(ScriptSourceCode(cachedScript));
        m_scriptElement->dispatchLoadEvent();
    }
    cachedScript->removeClient(this);
}

void ScriptElementData::notifyFinished(CachedResource* o)
{
    ASSERT_UNUSED(o, o == m_cachedScript);
    m_element->document()->asyncScriptRunner()->executeScriptSoon(this, m_cachedScript);
    m_cachedScript = 0;
}

bool ScriptElementData::ignoresLoadRequest() const
{
    return m_evaluated || m_requested || m_createdByParser || !m_element->inDocument();
}

bool ScriptElementData::shouldExecuteAsJavaScript() const
{
    /*
         Mozilla 1.8 accepts javascript1.0 - javascript1.7, but WinIE 7 accepts only javascript1.1 - javascript1.3.
         Mozilla 1.8 and WinIE 7 both accept javascript and livescript.
         WinIE 7 accepts ecmascript and jscript, but Mozilla 1.8 doesn't.
         Neither Mozilla 1.8 nor WinIE 7 accept leading or trailing whitespace.
         We want to accept all the values that either of these browsers accept, but not other values.
     
         FIXME: Is this HTML5 compliant?
     */
    String type = m_scriptElement->typeAttributeValue();
    if (!type.isEmpty()) {
        if (!MIMETypeRegistry::isSupportedJavaScriptMIMEType(type.stripWhiteSpace().lower()))
            return false;
    } else {
        String language = m_scriptElement->languageAttributeValue();
        if (!language.isEmpty() && !isSupportedJavaScriptLanguage(language))
            return false;
    }    

    // No type or language is specified, so we assume the script to be JavaScript.

    String forAttribute = m_scriptElement->forAttributeValue();
    String eventAttribute = m_scriptElement->eventAttributeValue();
    if (!forAttribute.isEmpty() && !eventAttribute.isEmpty()) {
        forAttribute = forAttribute.stripWhiteSpace();
        if (!equalIgnoringCase(forAttribute, "window"))
            return false;
            
        eventAttribute = eventAttribute.stripWhiteSpace();
        if (!equalIgnoringCase(eventAttribute, "onload") && !equalIgnoringCase(eventAttribute, "onload()"))
            return false;
    }
    
    return true;
}

String ScriptElementData::scriptCharset() const
{
    // First we try to get encoding from charset attribute.
    String charset = m_scriptElement->charsetAttributeValue().stripWhiteSpace();

    // If charset has not been declared in script tag, fall back to frame encoding.
    if (charset.isEmpty()) {
        if (Frame* frame = m_element->document()->frame())
            charset = frame->loader()->writer()->encoding();
    }

    return charset;
}

String ScriptElementData::scriptContent() const
{
    Vector<UChar> val;
    Text* firstTextNode = 0;
    bool foundMultipleTextNodes = false;

    for (Node* n = m_element->firstChild(); n; n = n->nextSibling()) {
        if (!n->isTextNode())
            continue;

        Text* t = static_cast<Text*>(n);
        if (foundMultipleTextNodes)
            append(val, t->data());
        else if (firstTextNode) {
            append(val, firstTextNode->data());
            append(val, t->data());
            foundMultipleTextNodes = true;
        } else
            firstTextNode = t;
    }

    if (firstTextNode && !foundMultipleTextNodes)
        return firstTextNode->data();

    return String::adopt(val);
}

bool ScriptElementData::isAsynchronous() const
{
    // Only external scripts may be asynchronous.
    // See: http://dev.w3.org/html5/spec/Overview.html#attr-script-async
    return !m_scriptElement->sourceAttributeValue().isEmpty() && m_scriptElement->asyncAttributeValue();
}

bool ScriptElementData::isDeferred() const
{
    // Only external scripts may be deferred and async trumps defer to allow for backward compatibility.
    // See: http://dev.w3.org/html5/spec/Overview.html#attr-script-defer
    return !m_scriptElement->sourceAttributeValue().isEmpty() && !m_scriptElement->asyncAttributeValue() && m_scriptElement->deferAttributeValue();
}

ScriptElement* toScriptElement(Element* element)
{
    if (element->isHTMLElement() && element->hasTagName(HTMLNames::scriptTag))
        return static_cast<HTMLScriptElement*>(element);

#if ENABLE(SVG)
    if (element->isSVGElement() && element->hasTagName(SVGNames::scriptTag))
        return static_cast<SVGScriptElement*>(element);
#endif

    return 0;
}

}
