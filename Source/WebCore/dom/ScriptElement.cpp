/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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
#include "DocumentInlines.h"
#include "ElementInlines.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLScriptElement.h"
#include "IgnoreDestructiveWriteCountIncrementer.h"
#include "InlineClassicScript.h"
#include "LoadableClassicScript.h"
#include "LoadableModuleScript.h"
#include "MIMETypeRegistry.h"
#include "ModuleFetchParameters.h"
#include "PendingScript.h"
#include "RuntimeApplicationChecks.h"
#include "SVGElementTypeHelpers.h"
#include "SVGScriptElement.h"
#include "ScriptController.h"
#include "ScriptDisallowedScope.h"
#include "ScriptRunner.h"
#include "ScriptSourceCode.h"
#include "ScriptableDocumentParser.h"
#include "Settings.h"
#include "TextNodeTraversal.h"
#include <wtf/SortedArrayMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/SystemTracing.h>

namespace WebCore {

static const auto maxUserGesturePropagationTime = 1_s;

ScriptElement::ScriptElement(Element& element, bool parserInserted, bool alreadyStarted)
    : m_element(element)
    , m_startLineNumber(OrdinalNumber::beforeFirst())
    , m_parserInserted(parserInserted ? ParserInserted::Yes : ParserInserted::No)
    , m_isExternalScript(false)
    , m_alreadyStarted(alreadyStarted)
    , m_haveFiredLoad(false)
    , m_errorOccurred(false)
    , m_willBeParserExecuted(false)
    , m_readyToBeParserExecuted(false)
    , m_willExecuteWhenDocumentFinishedParsing(false)
    , m_forceAsync(!parserInserted)
    , m_willExecuteInOrder(false)
    , m_isModuleScript(false)
    , m_creationTime(MonotonicTime::now())
    , m_userGestureToken(UserGestureIndicator::currentUserGesture())
{
    if (parserInserted) {
        Ref document = m_element.document();
        if (RefPtr parser = document->scriptableDocumentParser(); parser && !document->isInDocumentWrite())
            m_startLineNumber = parser->textPosition().m_line;
    }
}

void ScriptElement::didFinishInsertingNode()
{
    ASSERT(m_parserInserted == ParserInserted::No);
    prepareScript(); // FIXME: Provide a real starting line number here.
}

void ScriptElement::childrenChanged(const ContainerNode::ChildChange& childChange)
{
    if (m_parserInserted == ParserInserted::No && childChange.isInsertion() && m_element.isConnected())
        prepareScript(); // FIXME: Provide a real starting line number here.
}

void ScriptElement::handleSourceAttribute(const String& sourceURL)
{
    if (ignoresLoadRequest() || sourceURL.isEmpty())
        return;

    prepareScript(); // FIXME: Provide a real starting line number here.
}

void ScriptElement::handleAsyncAttribute()
{
    m_forceAsync = false;
}

static bool isLegacySupportedJavaScriptLanguage(const String& language)
{
    static constexpr ComparableLettersLiteral languageArray[] = {
        "ecmascript",
        "javascript",
        "javascript1.0",
        "javascript1.1",
        "javascript1.2",
        "javascript1.3",
        "javascript1.4",
        "javascript1.5",
        "javascript1.6",
        "javascript1.7",
        "jscript",
        "livescript",
    };
    static constexpr SortedArraySet languageSet { languageArray };
    return languageSet.contains(language);
}

void ScriptElement::dispatchErrorEvent()
{
    m_element.dispatchEvent(Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

// https://html.spec.whatwg.org/multipage/scripting.html#prepare-a-script
std::optional<ScriptElement::ScriptType> ScriptElement::determineScriptType(LegacyTypeSupport supportLegacyTypes) const
{
    // FIXME: isLegacySupportedJavaScriptLanguage() is not valid HTML5. It is used here to maintain backwards compatibility with existing layout tests. The specific violations are:
    // - Allowing type=javascript. type= should only support MIME types, such as text/javascript.
    // - Allowing a different set of languages for language= and type=. language= supports Javascript 1.1 and 1.4-1.6, but type= does not.
    String type = typeAttributeValue();
    String language = languageAttributeValue();
    if (type.isNull()) {
        if (language.isEmpty())
            return ScriptType::Classic; // Assume text/javascript.
        if (MIMETypeRegistry::isSupportedJavaScriptMIMEType("text/" + language))
            return ScriptType::Classic;
        if (isLegacySupportedJavaScriptLanguage(language))
            return ScriptType::Classic;
        return std::nullopt;
    }
    if (type.isEmpty())
        return ScriptType::Classic; // Assume text/javascript.

    if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(type.stripWhiteSpace()))
        return ScriptType::Classic;
    if (supportLegacyTypes == AllowLegacyTypeInTypeAttribute && isLegacySupportedJavaScriptLanguage(type))
        return ScriptType::Classic;

    // FIXME: XHTML spec defines "defer" attribute. But WebKit does not implement it for a long time.
    // And module tag also uses defer attribute semantics. We disable script type="module" for non HTML document.
    // Once "defer" is implemented, we can reconsider enabling modules in XHTML.
    // https://bugs.webkit.org/show_bug.cgi?id=123387
    if (!m_element.document().isHTMLDocument())
        return std::nullopt;

    // https://html.spec.whatwg.org/multipage/scripting.html#attr-script-type
    // Setting the attribute to an ASCII case-insensitive match for the string "module" means that the script is a module script.
    if (equalLettersIgnoringASCIICase(type, "module"))
        return ScriptType::Module;
    return std::nullopt;
}

// http://dev.w3.org/html5/spec/Overview.html#prepare-a-script
bool ScriptElement::prepareScript(const TextPosition& scriptStartPosition, LegacyTypeSupport supportLegacyTypes)
{
    if (m_alreadyStarted)
        return false;

    bool wasParserInserted;
    if (m_parserInserted == ParserInserted::Yes) {
        wasParserInserted = true;
        m_parserInserted = ParserInserted::No;
    } else
        wasParserInserted = false;

    if (wasParserInserted && !hasAsyncAttribute())
        m_forceAsync = true;

    auto sourceText = scriptContent();
    if (!hasSourceAttribute() && sourceText.isEmpty())
        return false;

    if (!m_element.isConnected())
        return false;

    ScriptType scriptType = ScriptType::Classic;
    if (std::optional<ScriptType> result = determineScriptType(supportLegacyTypes))
        scriptType = result.value();
    else
        return false;
    m_isModuleScript = scriptType == ScriptType::Module;

    if (wasParserInserted) {
        m_parserInserted = ParserInserted::Yes;
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

    if (scriptType == ScriptType::Classic && hasNoModuleAttribute())
        return false;

    m_preparationTimeDocumentIdentifier = document.identifier();

    if (!document.frame()->script().canExecuteScripts(AboutToExecuteScript))
        return false;

    if (scriptType == ScriptType::Classic && !isScriptForEventSupported())
        return false;

    // According to the spec, the module tag ignores the "charset" attribute as the same to the worker's
    // importScript. But WebKit supports the "charset" for importScript intentionally. So to be consistent,
    // even for the module tags, we handle the "charset" attribute.
    if (!charsetAttributeValue().isEmpty())
        m_characterEncoding = charsetAttributeValue();
    else
        m_characterEncoding = document.charset();

    if (scriptType == ScriptType::Classic) {
        if (hasSourceAttribute()) {
            if (!requestClassicScript(sourceAttributeValue()))
                return false;
        }
    } else {
        ASSERT(scriptType == ScriptType::Module);
        if (!requestModuleScript(scriptStartPosition))
            return false;
    }

    // All the inlined module script is handled by requestModuleScript. It produces LoadableModuleScript and inlined module script
    // is handled as the same to the external module script.

    bool isClassicExternalScript = scriptType == ScriptType::Classic && hasSourceAttribute();
    bool isParserInsertedDeferredScript = ((isClassicExternalScript && hasDeferAttribute()) || scriptType == ScriptType::Module)
        && m_parserInserted == ParserInserted::Yes && !hasAsyncAttribute();
    if (isParserInsertedDeferredScript) {
        m_willExecuteWhenDocumentFinishedParsing = true;
        m_willBeParserExecuted = true;
    } else if (isClassicExternalScript && m_parserInserted == ParserInserted::Yes && !hasAsyncAttribute()) {
        ASSERT(scriptType == ScriptType::Classic);
        m_willBeParserExecuted = true;
    } else if ((isClassicExternalScript || scriptType == ScriptType::Module) && !hasAsyncAttribute() && !m_forceAsync) {
        m_willExecuteInOrder = true;
        ASSERT(m_loadableScript);
        document.scriptRunner().queueScriptForExecution(*this, *m_loadableScript, ScriptRunner::IN_ORDER_EXECUTION);
    } else if (hasSourceAttribute() || scriptType == ScriptType::Module) {
        ASSERT(m_loadableScript);
        ASSERT(hasAsyncAttribute() || m_forceAsync);
        document.scriptRunner().queueScriptForExecution(*this, *m_loadableScript, ScriptRunner::ASYNC_EXECUTION);
    } else if (!hasSourceAttribute() && m_parserInserted == ParserInserted::Yes && !document.haveStylesheetsLoaded()) {
        ASSERT(scriptType == ScriptType::Classic);
        m_willBeParserExecuted = true;
        m_readyToBeParserExecuted = true;
    } else {
        ASSERT(scriptType == ScriptType::Classic);
        TextPosition position = document.isInDocumentWrite() ? TextPosition() : scriptStartPosition;
        executeClassicScript(ScriptSourceCode(sourceText, URL(document.url()), position, JSC::SourceProviderSourceType::Program, InlineClassicScript::create(*this)));
    }

    return true;
}

bool ScriptElement::requestClassicScript(const String& sourceURL)
{
    ASSERT(m_element.isConnected());
    ASSERT(!m_loadableScript);
    if (!stripLeadingAndTrailingHTMLSpaces(sourceURL).isEmpty()) {
        auto script = LoadableClassicScript::create(m_element.nonce(), m_element.attributeWithoutSynchronization(HTMLNames::integrityAttr), referrerPolicy(),
            m_element.attributeWithoutSynchronization(HTMLNames::crossoriginAttr), scriptCharset(), m_element.localName(), m_element.isInUserAgentShadowTree(), hasAsyncAttribute());

        auto scriptURL = m_element.document().completeURL(sourceURL);
        m_element.document().willLoadScriptElement(scriptURL);

        const auto& contentSecurityPolicy = *m_element.document().contentSecurityPolicy();
        if (!contentSecurityPolicy.allowNonParserInsertedScripts(scriptURL, URL(), m_startLineNumber, m_element.nonce(), String(), m_parserInserted))
            return false;

        if (script->load(m_element.document(), scriptURL)) {
            m_loadableScript = WTFMove(script);
            m_isExternalScript = true;
        }
    }

    if (m_loadableScript)
        return true;

    callOnMainThread([this, element = Ref<Element>(m_element)] {
        dispatchErrorEvent();
    });
    return false;
}

bool ScriptElement::requestModuleScript(const TextPosition& scriptStartPosition)
{
    // https://html.spec.whatwg.org/multipage/urls-and-fetching.html#cors-settings-attributes
    // Module is always CORS request. If attribute is not given, it should be same-origin credential.
    String nonce = m_element.nonce();
    String crossOriginMode = m_element.attributeWithoutSynchronization(HTMLNames::crossoriginAttr);
    if (crossOriginMode.isNull())
        crossOriginMode = ScriptElementCachedScriptFetcher::defaultCrossOriginModeForModule;

    if (hasSourceAttribute()) {
        ASSERT(m_element.isConnected());

        String sourceURL = sourceAttributeValue();
        if (stripLeadingAndTrailingHTMLSpaces(sourceURL).isEmpty()) {
            dispatchErrorEvent();
            return false;
        }

        auto moduleScriptRootURL = m_element.document().completeURL(sourceURL);
        if (!moduleScriptRootURL.isValid()) {
            dispatchErrorEvent();
            return false;
        }

        m_isExternalScript = true;
        auto script = LoadableModuleScript::create(nonce, m_element.attributeWithoutSynchronization(HTMLNames::integrityAttr), referrerPolicy(), crossOriginMode,
            scriptCharset(), m_element.localName(), m_element.isInUserAgentShadowTree());
        m_loadableScript = WTFMove(script);
        if (auto* frame = m_element.document().frame()) {
            auto& script = downcast<LoadableModuleScript>(*m_loadableScript.get());
            frame->script().loadModuleScript(script, moduleScriptRootURL.string(), script.parameters());
        }
        return true;
    }

    auto script = LoadableModuleScript::create(nonce, emptyString(), referrerPolicy(), crossOriginMode, scriptCharset(), m_element.localName(), m_element.isInUserAgentShadowTree());

    TextPosition position = m_element.document().isInDocumentWrite() ? TextPosition() : scriptStartPosition;
    ScriptSourceCode sourceCode(scriptContent(), URL(m_element.document().url()), position, JSC::SourceProviderSourceType::Module, script.copyRef());

    ASSERT(m_element.document().contentSecurityPolicy());
    const auto& contentSecurityPolicy = *m_element.document().contentSecurityPolicy();
    if (!contentSecurityPolicy.allowNonParserInsertedScripts(URL(), m_element.document().url(), m_startLineNumber, m_element.nonce(), sourceCode.source(), m_parserInserted))
        return false;

    if (!contentSecurityPolicy.allowInlineScript(m_element.document().url().string(), m_startLineNumber, sourceCode.source(), m_element, nonce, m_element.isInUserAgentShadowTree()))
        return false;

    m_loadableScript = WTFMove(script);
    if (auto* frame = m_element.document().frame())
        frame->script().loadModuleScript(downcast<LoadableModuleScript>(*m_loadableScript.get()), sourceCode);
    return true;
}

void ScriptElement::executeClassicScript(const ScriptSourceCode& sourceCode)
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isScriptAllowed() || !isInWebProcess());
    ASSERT(m_alreadyStarted);

    if (sourceCode.isEmpty())
        return;

    if (!m_isExternalScript) {
        ASSERT(m_element.document().contentSecurityPolicy());
        const ContentSecurityPolicy& contentSecurityPolicy = *m_element.document().contentSecurityPolicy();
        if (!contentSecurityPolicy.allowNonParserInsertedScripts(URL(), m_element.document().url(), m_startLineNumber, m_element.nonce(), sourceCode.source(), m_parserInserted))
            return;

        if (!contentSecurityPolicy.allowInlineScript(m_element.document().url().string(), m_startLineNumber, sourceCode.source(), m_element, m_element.nonce(), m_element.isInUserAgentShadowTree()))
            return;
    }

    auto& document = m_element.document();
    auto* frame = document.frame();
    if (!frame)
        return;

    IgnoreDestructiveWriteCountIncrementer ignoreDestructiveWriteCountIncrementer(m_isExternalScript ? &document : nullptr);
    CurrentScriptIncrementer currentScriptIncrementer(document, *this);

    WTFBeginSignpost(this, "Execute Script Element", "executing classic script from URL: %{public}s async: %d defer: %d", m_isExternalScript ? sourceCode.url().string().utf8().data() : "inline", hasAsyncAttribute(), hasDeferAttribute());
    frame->script().evaluateIgnoringException(sourceCode);
    WTFEndSignpost(this, "Execute Script Element");
}

void ScriptElement::executeModuleScript(LoadableModuleScript& loadableModuleScript)
{
    // https://html.spec.whatwg.org/multipage/scripting.html#execute-the-script-block

    ASSERT(!loadableModuleScript.error());

    auto& document = m_element.document();
    auto* frame = document.frame();
    if (!frame)
        return;

    IgnoreDestructiveWriteCountIncrementer ignoreDestructiveWriteCountIncrementer(&document);
    CurrentScriptIncrementer currentScriptIncrementer(document, *this);

    WTFBeginSignpost(this, "Execute Script Element", "executing module script");
    frame->script().linkAndEvaluateModuleScript(loadableModuleScript);
    WTFEndSignpost(this, "Execute Script Element", "executing module script");
}

void ScriptElement::dispatchLoadEventRespectingUserGestureIndicator()
{
    if (MonotonicTime::now() - m_creationTime > maxUserGesturePropagationTime) {
        dispatchLoadEvent();
        return;
    }

    UserGestureIndicator indicator(m_userGestureToken);
    dispatchLoadEvent();
}

void ScriptElement::executeScriptAndDispatchEvent(LoadableScript& loadableScript)
{
    if (std::optional<LoadableScript::Error> error = loadableScript.error()) {
        if (error->errorValue) {
            // https://html.spec.whatwg.org/multipage/webappapis.html#report-the-exception
            // An error value is present when there is a load failure that was
            // not triggered during fetching. In this case, we need to report
            // the exception to the global object.
            if (auto* frame = m_element.document().frame())
                frame->script().reportExceptionFromScriptError(error.value(), loadableScript.isModuleScript());
        } else {
            // https://html.spec.whatwg.org/multipage/scripting.html#execute-the-script-block
            // When the script is "null" due to a fetch error, an error event
            // should be dispatched for the script element.
            if (std::optional<LoadableScript::ConsoleMessage> message = error->consoleMessage)
                m_element.document().addConsoleMessage(message->source, message->level, message->message);
            dispatchErrorEvent();
        }
    } else if (!loadableScript.wasCanceled()) {
        ASSERT(!loadableScript.error());
        loadableScript.execute(*this);
        dispatchLoadEventRespectingUserGestureIndicator();
    }
}

void ScriptElement::executePendingScript(PendingScript& pendingScript)
{
    if (m_element.document().identifier() != m_preparationTimeDocumentIdentifier) {
        m_element.document().addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Not executing script because it moved between documents during fetching"_s);
        return;
    }

    if (auto* loadableScript = pendingScript.loadableScript())
        executeScriptAndDispatchEvent(*loadableScript);
    else {
        ASSERT(!pendingScript.error());
        ASSERT_WITH_MESSAGE(scriptType() == ScriptType::Classic, "Module script always have a loadableScript pointer.");
        executeClassicScript(ScriptSourceCode(scriptContent(), URL(m_element.document().url()), pendingScript.startingPosition(), JSC::SourceProviderSourceType::Program, InlineClassicScript::create(*this)));
        dispatchLoadEventRespectingUserGestureIndicator();
    }
}

bool ScriptElement::ignoresLoadRequest() const
{
    return m_alreadyStarted || m_isExternalScript || m_parserInserted == ParserInserted::Yes || !m_element.isConnected();
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
    return TextNodeTraversal::childTextContent(m_element);
}

void ScriptElement::ref()
{
    m_element.ref();
}

void ScriptElement::deref()
{
    m_element.deref();
}

bool isScriptElement(Element& element)
{
    return is<HTMLScriptElement>(element) || is<SVGScriptElement>(element);
}

ScriptElement& downcastScriptElement(Element& element)
{
    if (is<HTMLScriptElement>(element))
        return downcast<HTMLScriptElement>(element);
    return downcast<SVGScriptElement>(element);
}

}
