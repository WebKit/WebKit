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
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "IgnoreDestructiveWriteCountIncrementer.h"
#include "InlineClassicScript.h"
#include "LoadableClassicScript.h"
#include "LoadableModuleScript.h"
#include "MIMETypeRegistry.h"
#include "PendingScript.h"
#include "RuntimeApplicationChecks.h"
#include "SVGScriptElement.h"
#include "ScriptController.h"
#include "ScriptDisallowedScope.h"
#include "ScriptRunner.h"
#include "ScriptSourceCode.h"
#include "ScriptableDocumentParser.h"
#include "Settings.h"
#include "TextNodeTraversal.h"
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

static const auto maxUserGesturePropagationTime = 1_s;

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
    , m_isModuleScript(false)
    , m_creationTime(MonotonicTime::now())
    , m_userGestureToken(UserGestureIndicator::currentUserGesture())
{
    if (parserInserted && m_element.document().scriptableDocumentParser() && !m_element.document().isInDocumentWrite())
        m_startLineNumber = m_element.document().scriptableDocumentParser()->textPosition().m_line;
}

void ScriptElement::didFinishInsertingNode()
{
    ASSERT(!m_parserInserted);
    prepareScript(); // FIXME: Provide a real starting line number here.
}

void ScriptElement::childrenChanged(const ContainerNode::ChildChange& childChange)
{
    if (!m_parserInserted && childChange.isInsertion() && m_element.isConnected())
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
    static const auto languages = makeNeverDestroyed(HashSet<String, ASCIICaseInsensitiveHash> {
        "javascript",
        "javascript1.0",
        "javascript1.1",
        "javascript1.2",
        "javascript1.3",
        "javascript1.4",
        "javascript1.5",
        "javascript1.6",
        "javascript1.7",
        "livescript",
        "ecmascript",
        "jscript",
    });
    return languages.get().contains(language);
}

void ScriptElement::dispatchErrorEvent()
{
    m_element.dispatchEvent(Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

Optional<ScriptElement::ScriptType> ScriptElement::determineScriptType(LegacyTypeSupport supportLegacyTypes) const
{
    // FIXME: isLegacySupportedJavaScriptLanguage() is not valid HTML5. It is used here to maintain backwards compatibility with existing layout tests. The specific violations are:
    // - Allowing type=javascript. type= should only support MIME types, such as text/javascript.
    // - Allowing a different set of languages for language= and type=. language= supports Javascript 1.1 and 1.4-1.6, but type= does not.
    String type = typeAttributeValue();
    String language = languageAttributeValue();
    if (type.isEmpty()) {
        if (language.isEmpty())
            return ScriptType::Classic; // Assume text/javascript.
        if (MIMETypeRegistry::isSupportedJavaScriptMIMEType("text/" + language))
            return ScriptType::Classic;
        if (isLegacySupportedJavaScriptLanguage(language))
            return ScriptType::Classic;
        return WTF::nullopt;
    }
    if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(type.stripWhiteSpace()))
        return ScriptType::Classic;
    if (supportLegacyTypes == AllowLegacyTypeInTypeAttribute && isLegacySupportedJavaScriptLanguage(type))
        return ScriptType::Classic;

    // FIXME: XHTML spec defines "defer" attribute. But WebKit does not implement it for a long time.
    // And module tag also uses defer attribute semantics. We disable script type="module" for non HTML document.
    // Once "defer" is implemented, we can reconsider enabling modules in XHTML.
    // https://bugs.webkit.org/show_bug.cgi?id=123387
    if (!m_element.document().isHTMLDocument())
        return WTF::nullopt;

    // https://html.spec.whatwg.org/multipage/scripting.html#attr-script-type
    // Setting the attribute to an ASCII case-insensitive match for the string "module" means that the script is a module script.
    if (equalLettersIgnoringASCIICase(type, "module"))
        return ScriptType::Module;
    return WTF::nullopt;
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

    if (wasParserInserted && !hasAsyncAttribute())
        m_forceAsync = true;

    // FIXME: HTML5 spec says we should check that all children are either comments or empty text nodes.
    if (!hasSourceAttribute() && !m_element.firstChild())
        return false;

    if (!m_element.isConnected())
        return false;

    ScriptType scriptType = ScriptType::Classic;
    if (Optional<ScriptType> result = determineScriptType(supportLegacyTypes))
        scriptType = result.value();
    else
        return false;
    m_isModuleScript = scriptType == ScriptType::Module;

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

    if (scriptType == ScriptType::Classic && hasNoModuleAttribute())
        return false;

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
        && m_parserInserted && !hasAsyncAttribute();
    if (isParserInsertedDeferredScript) {
        m_willExecuteWhenDocumentFinishedParsing = true;
        m_willBeParserExecuted = true;
    } else if (isClassicExternalScript && m_parserInserted && !hasAsyncAttribute()) {
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
    } else if (!hasSourceAttribute() && m_parserInserted && !document.haveStylesheetsLoaded()) {
        ASSERT(scriptType == ScriptType::Classic);
        m_willBeParserExecuted = true;
        m_readyToBeParserExecuted = true;
    } else {
        ASSERT(scriptType == ScriptType::Classic);
        TextPosition position = document.isInDocumentWrite() ? TextPosition() : scriptStartPosition;
        executeClassicScript(ScriptSourceCode(scriptContent(), URL(document.url()), position, JSC::SourceProviderSourceType::Program, InlineClassicScript::create(*this)));
    }

    return true;
}

bool ScriptElement::requestClassicScript(const String& sourceURL)
{
    Ref<Document> originalDocument(m_element.document());
    if (!m_element.dispatchBeforeLoadEvent(sourceURL))
        return false;
    bool didEventListenerDisconnectThisElement = !m_element.isConnected() || &m_element.document() != originalDocument.ptr();
    if (didEventListenerDisconnectThisElement)
        return false;

    ASSERT(!m_loadableScript);
    if (!stripLeadingAndTrailingHTMLSpaces(sourceURL).isEmpty()) {
        auto script = LoadableClassicScript::create(
            m_element.attributeWithoutSynchronization(HTMLNames::nonceAttr),
            m_element.document().settings().subresourceIntegrityEnabled() ? m_element.attributeWithoutSynchronization(HTMLNames::integrityAttr).string() : emptyString(),
            m_element.attributeWithoutSynchronization(HTMLNames::crossoriginAttr),
            scriptCharset(),
            m_element.localName(),
            m_element.isInUserAgentShadowTree());
        if (script->load(m_element.document(), m_element.document().completeURL(sourceURL))) {
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
    String nonce = m_element.attributeWithoutSynchronization(HTMLNames::nonceAttr);
    String crossOriginMode = m_element.attributeWithoutSynchronization(HTMLNames::crossoriginAttr);
    if (crossOriginMode.isNull())
        crossOriginMode = "omit"_s;

    if (hasSourceAttribute()) {
        String sourceURL = sourceAttributeValue();
        Ref<Document> originalDocument(m_element.document());
        if (!m_element.dispatchBeforeLoadEvent(sourceURL))
            return false;

        bool didEventListenerDisconnectThisElement = !m_element.isConnected() || &m_element.document() != originalDocument.ptr();
        if (didEventListenerDisconnectThisElement)
            return false;

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
        auto script = LoadableModuleScript::create(
            nonce,
            m_element.document().settings().subresourceIntegrityEnabled() ? m_element.attributeWithoutSynchronization(HTMLNames::integrityAttr).string() : emptyString(),
            crossOriginMode,
            scriptCharset(),
            m_element.localName(),
            m_element.isInUserAgentShadowTree());
        script->load(m_element.document(), moduleScriptRootURL);
        m_loadableScript = WTFMove(script);
        return true;
    }

    auto script = LoadableModuleScript::create(nonce, emptyString(), crossOriginMode, scriptCharset(), m_element.localName(), m_element.isInUserAgentShadowTree());

    TextPosition position = m_element.document().isInDocumentWrite() ? TextPosition() : scriptStartPosition;
    ScriptSourceCode sourceCode(scriptContent(), URL(m_element.document().url()), position, JSC::SourceProviderSourceType::Module, script.copyRef());

    ASSERT(m_element.document().contentSecurityPolicy());
    const auto& contentSecurityPolicy = *m_element.document().contentSecurityPolicy();
    bool hasKnownNonce = contentSecurityPolicy.allowScriptWithNonce(nonce, m_element.isInUserAgentShadowTree());
    if (!contentSecurityPolicy.allowInlineScript(m_element.document().url(), m_startLineNumber, sourceCode.source().toStringWithoutCopying(), hasKnownNonce))
        return false;

    script->load(m_element.document(), sourceCode);
    m_loadableScript = WTFMove(script);
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
        bool hasKnownNonce = contentSecurityPolicy.allowScriptWithNonce(m_element.attributeWithoutSynchronization(HTMLNames::nonceAttr), m_element.isInUserAgentShadowTree());
        if (!contentSecurityPolicy.allowInlineScript(m_element.document().url(), m_startLineNumber, sourceCode.source().toStringWithoutCopying(), hasKnownNonce))
            return;
    }

    auto& document = m_element.document();
    auto* frame = document.frame();
    if (!frame)
        return;

    IgnoreDestructiveWriteCountIncrementer ignoreDesctructiveWriteCountIncrementer(m_isExternalScript ? &document : nullptr);
    CurrentScriptIncrementer currentScriptIncrementer(document, m_element);

    frame->script().evaluate(sourceCode);
}

void ScriptElement::executeModuleScript(LoadableModuleScript& loadableModuleScript)
{
    // https://html.spec.whatwg.org/multipage/scripting.html#execute-the-script-block

    ASSERT(!loadableModuleScript.error());

    auto& document = m_element.document();
    auto* frame = document.frame();
    if (!frame)
        return;

    IgnoreDestructiveWriteCountIncrementer ignoreDesctructiveWriteCountIncrementer(&document);
    CurrentScriptIncrementer currentScriptIncrementer(document, m_element);

    frame->script().linkAndEvaluateModuleScript(loadableModuleScript);
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
    if (Optional<LoadableScript::Error> error = loadableScript.error()) {
        if (Optional<LoadableScript::ConsoleMessage> message = error->consoleMessage)
            m_element.document().addConsoleMessage(message->source, message->level, message->message);
        dispatchErrorEvent();
    } else if (!loadableScript.wasCanceled()) {
        ASSERT(!loadableScript.error());
        loadableScript.execute(*this);
        dispatchLoadEventRespectingUserGestureIndicator();
    }
}

void ScriptElement::executePendingScript(PendingScript& pendingScript)
{
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
    return m_alreadyStarted || m_isExternalScript || m_parserInserted || !m_element.isConnected();
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
