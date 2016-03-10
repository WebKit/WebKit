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

#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CachedScript.h"
#include "ContentSecurityPolicy.h"
#include "CrossOriginAccessControl.h"
#include "CurrentScriptIncrementer.h"
#include "Event.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "IgnoreDestructiveWriteCountIncrementer.h"
#include "MIMETypeRegistry.h"
#include "Page.h"
#include "SVGNames.h"
#include "SVGScriptElement.h"
#include "ScriptController.h"
#include "ScriptRunner.h"
#include "ScriptSourceCode.h"
#include "ScriptableDocumentParser.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "TextNodeTraversal.h"
#include <bindings/ScriptValue.h>
#include <inspector/ScriptCallStack.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

ScriptElement::ScriptElement(Element& element, bool parserInserted, bool alreadyStarted)
    : m_element(element)
    , m_startLineNumber(WTF::OrdinalNumber::beforeFirst())
    , m_parserInserted(parserInserted)
    , m_isExternalScript(false)
    , m_alreadyStarted(alreadyStarted)
    , m_haveFiredLoad(false)
    , m_willBeParserExecuted(false)
    , m_readyToBeParserExecuted(false)
    , m_willExecuteWhenDocumentFinishedParsing(false)
    , m_forceAsync(!parserInserted)
    , m_willExecuteInOrder(false)
    , m_requestUsesAccessControl(false)
{
    if (parserInserted && m_element.document().scriptableDocumentParser() && !m_element.document().isInDocumentWrite())
        m_startLineNumber = m_element.document().scriptableDocumentParser()->textPosition().m_line;
}

ScriptElement::~ScriptElement()
{
    stopLoadRequest();
}

bool ScriptElement::shouldCallFinishedInsertingSubtree(ContainerNode& insertionPoint)
{
    return insertionPoint.inDocument() && !m_parserInserted;
}

void ScriptElement::finishedInsertingSubtree()
{
    ASSERT(!m_parserInserted);
    prepareScript(); // FIXME: Provide a real starting line number here.
}

void ScriptElement::childrenChanged()
{
    if (!m_parserInserted && m_element.inDocument())
        prepareScript(); // FIXME: Provide a real starting line number here.
}

void ScriptElement::handleSourceAttribute(const String& sourceUrl)
{
    if (ignoresLoadRequest() || sourceUrl.isEmpty())
        return;

    prepareScript(); // FIXME: Provide a real starting line number here.
}

void ScriptElement::handleAsyncAttribute()
{
    m_forceAsync = false;
}

// Helper function
static bool isLegacySupportedJavaScriptLanguage(const String& language)
{
    // Mozilla 1.8 accepts javascript1.0 - javascript1.7, but WinIE 7 accepts only javascript1.1 - javascript1.3.
    // Mozilla 1.8 and WinIE 7 both accept javascript and livescript.
    // WinIE 7 accepts ecmascript and jscript, but Mozilla 1.8 doesn't.
    // Neither Mozilla 1.8 nor WinIE 7 accept leading or trailing whitespace.
    // We want to accept all the values that either of these browsers accept, but not other values.

    // FIXME: This function is not HTML5 compliant. These belong in the MIME registry as "text/javascript<version>" entries.
    typedef HashSet<String, ASCIICaseInsensitiveHash> LanguageSet;
    static NeverDestroyed<LanguageSet> languages;
    if (languages.get().isEmpty()) {
        languages.get().add("javascript");
        languages.get().add("javascript");
        languages.get().add("javascript1.0");
        languages.get().add("javascript1.1");
        languages.get().add("javascript1.2");
        languages.get().add("javascript1.3");
        languages.get().add("javascript1.4");
        languages.get().add("javascript1.5");
        languages.get().add("javascript1.6");
        languages.get().add("javascript1.7");
        languages.get().add("livescript");
        languages.get().add("ecmascript");
        languages.get().add("jscript");
    }

    return languages.get().contains(language);
}

void ScriptElement::dispatchErrorEvent()
{
    m_element.dispatchEvent(Event::create(eventNames().errorEvent, false, false));
}

bool ScriptElement::isScriptTypeSupported(LegacyTypeSupport supportLegacyTypes) const
{
    // FIXME: isLegacySupportedJavaScriptLanguage() is not valid HTML5. It is used here to maintain backwards compatibility with existing layout tests. The specific violations are:
    // - Allowing type=javascript. type= should only support MIME types, such as text/javascript.
    // - Allowing a different set of languages for language= and type=. language= supports Javascript 1.1 and 1.4-1.6, but type= does not.
    String type = typeAttributeValue();
    String language = languageAttributeValue();
    if (type.isEmpty()) {
        if (language.isEmpty())
            return true; // Assume text/javascript.
        if (MIMETypeRegistry::isSupportedJavaScriptMIMEType("text/" + language))
            return true;
        if (isLegacySupportedJavaScriptLanguage(language))
            return true;
        return false;
    }
    if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(type.stripWhiteSpace()))
        return true;
    if (supportLegacyTypes == AllowLegacyTypeInTypeAttribute && isLegacySupportedJavaScriptLanguage(type))
        return true;
    return false;
}

// http://dev.w3.org/html5/spec/Overview.html#prepare-a-script
bool ScriptElement::prepareScript(const TextPosition& scriptStartPosition, LegacyTypeSupport supportLegacyTypes)
{
    if (m_alreadyStarted)
        return false;

    bool wasParserInserted;
    if (m_parserInserted) {
        wasParserInserted = true;
        m_parserInserted = false;
    } else
        wasParserInserted = false;

    if (wasParserInserted && !asyncAttributeValue())
        m_forceAsync = true;

    // FIXME: HTML5 spec says we should check that all children are either comments or empty text nodes.
    if (!hasSourceAttribute() && !m_element.firstChild())
        return false;

    if (!m_element.inDocument())
        return false;

    if (!isScriptTypeSupported(supportLegacyTypes))
        return false;

    if (wasParserInserted) {
        m_parserInserted = true;
        m_forceAsync = false;
    }

    m_alreadyStarted = true;

    // FIXME: If script is parser inserted, verify it's still in the original document.
    Document& document = m_element.document();

    // FIXME: Eventually we'd like to evaluate scripts which are inserted into a
    // viewless document but this'll do for now.
    // See http://bugs.webkit.org/show_bug.cgi?id=5727
    if (!document.frame())
        return false;

    if (!document.frame()->script().canExecuteScripts(AboutToExecuteScript))
        return false;

    if (!isScriptForEventSupported())
        return false;

    if (!charsetAttributeValue().isEmpty())
        m_characterEncoding = charsetAttributeValue();
    else
        m_characterEncoding = document.charset();

    if (hasSourceAttribute())
        if (!requestScript(sourceAttributeValue()))
            return false;

    if (hasSourceAttribute() && deferAttributeValue() && m_parserInserted && !asyncAttributeValue()) {
        m_willExecuteWhenDocumentFinishedParsing = true;
        m_willBeParserExecuted = true;
    } else if (hasSourceAttribute() && m_parserInserted && !asyncAttributeValue())
        m_willBeParserExecuted = true;
    else if (!hasSourceAttribute() && m_parserInserted && !document.haveStylesheetsLoaded()) {
        m_willBeParserExecuted = true;
        m_readyToBeParserExecuted = true;
    } else if (hasSourceAttribute() && !asyncAttributeValue() && !m_forceAsync) {
        m_willExecuteInOrder = true;
        document.scriptRunner()->queueScriptForExecution(this, m_cachedScript, ScriptRunner::IN_ORDER_EXECUTION);
        m_cachedScript->addClient(this);
    } else if (hasSourceAttribute()) {
        m_element.document().scriptRunner()->queueScriptForExecution(this, m_cachedScript, ScriptRunner::ASYNC_EXECUTION);
        m_cachedScript->addClient(this);
    } else {
        // Reset line numbering for nested writes.
        TextPosition position = document.isInDocumentWrite() ? TextPosition() : scriptStartPosition;
        executeScript(ScriptSourceCode(scriptContent(), document.url(), position));
    }

    return true;
}

bool ScriptElement::requestScript(const String& sourceUrl)
{
    Ref<Document> originalDocument(m_element.document());
    if (!m_element.dispatchBeforeLoadEvent(sourceUrl))
        return false;
    if (!m_element.inDocument() || &m_element.document() != originalDocument.ptr())
        return false;

    ASSERT(!m_cachedScript);
    if (!stripLeadingAndTrailingHTMLSpaces(sourceUrl).isEmpty()) {
        ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
        options.setContentSecurityPolicyImposition(m_element.isInUserAgentShadowTree() ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck);

        CachedResourceRequest request(ResourceRequest(m_element.document().completeURL(sourceUrl)), options);

        String crossOriginMode = m_element.fastGetAttribute(HTMLNames::crossoriginAttr);
        if (!crossOriginMode.isNull()) {
            m_requestUsesAccessControl = true;
            StoredCredentials allowCredentials = equalLettersIgnoringASCIICase(crossOriginMode, "use-credentials") ? AllowStoredCredentials : DoNotAllowStoredCredentials;
            updateRequestForAccessControl(request.mutableResourceRequest(), m_element.document().securityOrigin(), allowCredentials);
        }
        request.setCharset(scriptCharset());
        request.setInitiator(&element());

        m_cachedScript = m_element.document().cachedResourceLoader().requestScript(request);
        m_isExternalScript = true;
    }

    if (m_cachedScript)
        return true;

    RefPtr<Element> element = &m_element;
    callOnMainThread([this, element] {
        dispatchErrorEvent();
    });
    return false;
}

void ScriptElement::executeScript(const ScriptSourceCode& sourceCode)
{
    ASSERT(m_alreadyStarted);

    if (sourceCode.isEmpty())
        return;

    if (!m_isExternalScript && !m_element.document().contentSecurityPolicy()->allowInlineScript(m_element.document().url(), m_startLineNumber, sourceCode.source().toStringWithoutCopying(), m_element.isInUserAgentShadowTree()))
        return;

#if ENABLE(NOSNIFF)
    if (m_isExternalScript && m_cachedScript && !m_cachedScript->mimeTypeAllowedByNosniff()) {
        m_element.document().addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Refused to execute script from '" + m_cachedScript->url().stringCenterEllipsizedToLength() + "' because its MIME type ('" + m_cachedScript->mimeType() + "') is not executable, and strict MIME type checking is enabled.");
        return;
    }
#endif

    Ref<Document> document(m_element.document());
    if (Frame* frame = document->frame()) {
        IgnoreDestructiveWriteCountIncrementer ignoreDesctructiveWriteCountIncrementer(m_isExternalScript ? document.ptr() : nullptr);
        CurrentScriptIncrementer currentScriptIncrementer(document, &m_element);

        // Create a script from the script element node, using the script
        // block's source and the script block's type.
        // Note: This is where the script is compiled and actually executed.
        frame->script().evaluate(sourceCode);
    }
}

void ScriptElement::stopLoadRequest()
{
    if (m_cachedScript) {
        if (!m_willBeParserExecuted)
            m_cachedScript->removeClient(this);
        m_cachedScript = nullptr;
    }
}

void ScriptElement::execute(CachedScript* cachedScript)
{
    ASSERT(!m_willBeParserExecuted);
    ASSERT(cachedScript);
    if (cachedScript->errorOccurred())
        dispatchErrorEvent();
    else if (!cachedScript->wasCanceled()) {
        executeScript(ScriptSourceCode(cachedScript));
        dispatchLoadEvent();
    }
    cachedScript->removeClient(this);
}

void ScriptElement::notifyFinished(CachedResource* resource)
{
    ASSERT(!m_willBeParserExecuted);

    // CachedResource possibly invokes this notifyFinished() more than
    // once because ScriptElement doesn't unsubscribe itself from
    // CachedResource here and does it in execute() instead.
    // We use m_cachedScript to check if this function is already called.
    ASSERT_UNUSED(resource, resource == m_cachedScript);
    if (!m_cachedScript)
        return;

    if (m_requestUsesAccessControl && !m_cachedScript->passesSameOriginPolicyCheck(*m_element.document().securityOrigin())) {
        dispatchErrorEvent();
        static NeverDestroyed<String> consoleMessage(ASCIILiteral("Cross-origin script load denied by Cross-Origin Resource Sharing policy."));
        m_element.document().addConsoleMessage(MessageSource::JS, MessageLevel::Error, consoleMessage);
        return;
    }

    if (m_willExecuteInOrder)
        m_element.document().scriptRunner()->notifyScriptReady(this, ScriptRunner::IN_ORDER_EXECUTION);
    else
        m_element.document().scriptRunner()->notifyScriptReady(this, ScriptRunner::ASYNC_EXECUTION);

    m_cachedScript = nullptr;
}

bool ScriptElement::ignoresLoadRequest() const
{
    return m_alreadyStarted || m_isExternalScript || m_parserInserted || !m_element.inDocument();
}

bool ScriptElement::isScriptForEventSupported() const
{
    String eventAttribute = eventAttributeValue();
    String forAttribute = forAttributeValue();
    if (!eventAttribute.isNull() && !forAttribute.isNull()) {
        forAttribute = stripLeadingAndTrailingHTMLSpaces(forAttribute);
        if (!equalLettersIgnoringASCIICase(forAttribute, "window"))
            return false;

        eventAttribute = stripLeadingAndTrailingHTMLSpaces(eventAttribute);
        if (!equalLettersIgnoringASCIICase(eventAttribute, "onload") && !equalLettersIgnoringASCIICase(eventAttribute, "onload()"))
            return false;
    }
    return true;
}

String ScriptElement::scriptContent() const
{
    StringBuilder result;
    for (auto* text = TextNodeTraversal::firstChild(m_element); text; text = TextNodeTraversal::nextSibling(*text))
        result.append(text->data());
    return result.toString();
}

ScriptElement* toScriptElementIfPossible(Element* element)
{
    if (is<HTMLScriptElement>(*element))
        return downcast<HTMLScriptElement>(element);

    if (is<SVGScriptElement>(*element))
        return downcast<SVGScriptElement>(element);

    return nullptr;
}

}
