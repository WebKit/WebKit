/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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
#include "CommonVM.h"
#include "ContentSecurityPolicy.h"
#include "CrossOriginAccessControl.h"
#include "CurrentScriptIncrementer.h"
#include "DocumentInlines.h"
#include "ElementInlines.h"
#include "Event.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "FrameLoader.h"
#include "HTMLNames.h"
#include "HTMLScriptElement.h"
#include "IgnoreDestructiveWriteCountIncrementer.h"
#include "InlineClassicScript.h"
#include "LoadableClassicScript.h"
#include "LoadableImportMap.h"
#include "LoadableModuleScript.h"
#include "LocalFrame.h"
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
#include "TrustedType.h"
#include <JavaScriptCore/ImportMap.h>
#include <wtf/Scope.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

static const auto maxUserGesturePropagationTime = 1_s;

ScriptElement::ScriptElement(Element& element, bool parserInserted, bool alreadyStarted)
    : m_element(element)
    , m_parserInserted(parserInserted ? ParserInserted::Yes : ParserInserted::No)
    , m_alreadyStarted(alreadyStarted)
    , m_forceAsync(!parserInserted)
    , m_creationTime(MonotonicTime::now())
    , m_userGestureToken(UserGestureIndicator::currentUserGesture())
{
    Ref vm = commonVM();
    m_taintedOrigin = computeNewSourceTaintedOriginFromStack(vm, vm->topCallFrame);
    if (parserInserted) {
        Ref document = element.document();
        if (RefPtr parser = document->scriptableDocumentParser(); parser && !document->isInDocumentWrite())
            m_startLineNumber = parser->textPosition().m_line;
    }
}

void ScriptElement::didFinishInsertingNode()
{
    if (m_parserInserted == ParserInserted::No)
        prepareScript(); // FIXME: Provide a real starting line number here.
}

void ScriptElement::childrenChanged(const ContainerNode::ChildChange& childChange)
{
    if (m_parserInserted == ParserInserted::No && childChange.isInsertion() && element().isConnected())
        prepareScript(); // FIXME: Provide a real starting line number here.

    if (childChange.source == ContainerNode::ChildChange::Source::API)
        m_childrenChangedByAPI = true;
}

void ScriptElement::finishParsingChildren()
{
    if (!m_childrenChangedByAPI)
        m_trustedScriptText = scriptContent();
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

void ScriptElement::dispatchErrorEvent()
{
    protectedElement()->dispatchEvent(Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

// https://html.spec.whatwg.org/multipage/scripting.html#prepare-a-script
std::optional<ScriptType> ScriptElement::determineScriptType(const String& type, const String& language, bool isHTMLDocument)
{
    if (type.isNull()) {
        if (language.isEmpty())
            return ScriptType::Classic;
        if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(makeString("text/"_s, language)))
            return ScriptType::Classic;
        return std::nullopt;
    }
    if (type.isEmpty())
        return ScriptType::Classic; // Assume text/javascript.

    if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(type.trim(isASCIIWhitespace)))
        return ScriptType::Classic;

    // FIXME: XHTML spec defines "defer" attribute. But WebKit does not implement it for a long time.
    // And module tag also uses defer attribute semantics. We disable script type="module" for non HTML document.
    // Once "defer" is implemented, we can reconsider enabling modules in XHTML.
    // https://bugs.webkit.org/show_bug.cgi?id=123387
    if (!isHTMLDocument)
        return std::nullopt;

    // https://html.spec.whatwg.org/multipage/scripting.html#attr-script-type
    // Setting the attribute to an ASCII case-insensitive match for the string "module" means that the script is a module script.
    if (equalLettersIgnoringASCIICase(type, "module"_s))
        return ScriptType::Module;

    // https://wicg.github.io/import-maps/#integration-prepare-a-script
    // If the script block’s type string is an ASCII case-insensitive match for the string "importmap", the script’s type is "importmap".
    if (equalLettersIgnoringASCIICase(type, "importmap"_s))
        return ScriptType::ImportMap;

    return std::nullopt;
}

std::optional<ScriptType> ScriptElement::determineScriptType() const
{
    return determineScriptType(typeAttributeValue(), languageAttributeValue(), element().document().isHTMLDocument());
}

// https://html.spec.whatwg.org/multipage/scripting.html#prepare-the-script-element
bool ScriptElement::prepareScript(const TextPosition& scriptStartPosition)
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

    String sourceText = scriptContent();
    Ref context = *element().scriptExecutionContext();
    if (context->settingsValues().trustedTypesEnabled && sourceText != m_trustedScriptText) {
        auto trustedText = trustedTypeCompliantString(TrustedType::TrustedScript, context, sourceText, is<HTMLScriptElement>(element()) ? "HTMLScriptElement text"_s : "SVGScriptElement text"_s);
        if (trustedText.hasException())
            return false;
        sourceText = trustedText.releaseReturnValue();
    }

    if (!hasSourceAttribute() && sourceText.isEmpty())
        return false;

    if (!element().isConnected())
        return false;

    ScriptType scriptType = ScriptType::Classic;
    if (std::optional<ScriptType> result = determineScriptType())
        scriptType = result.value();
    else
        return false;
    m_scriptType = scriptType;

    if (wasParserInserted) {
        m_parserInserted = ParserInserted::Yes;
        m_forceAsync = false;
    }

    m_alreadyStarted = true;

    auto element = protectedElement();
    // FIXME: If script is parser inserted, verify it's still in the original document.
    Ref document = element->document();

    // FIXME: Eventually we'd like to evaluate scripts which are inserted into a
    // viewless document but this'll do for now.
    // See http://bugs.webkit.org/show_bug.cgi?id=5727
    if (!document->frame())
        return false;

    if (scriptType == ScriptType::Classic && hasNoModuleAttribute())
        return false;

    m_preparationTimeDocumentIdentifier = document->identifier();

    if (!document->frame()->script().canExecuteScripts(ReasonForCallingCanExecuteScripts::AboutToExecuteScript))
        return false;

    if (scriptType == ScriptType::Classic && isScriptPreventedByAttributes())
        return false;

    // According to the spec, the module tag ignores the "charset" attribute as the same to the worker's
    // importScript. But WebKit supports the "charset" for importScript intentionally. So to be consistent,
    // even for the module tags, we handle the "charset" attribute.
    if (!charsetAttributeValue().isEmpty())
        m_characterEncoding = charsetAttributeValue();
    else
        m_characterEncoding = document->charset();

    switch (scriptType) {
    case ScriptType::Classic: {
        if (hasSourceAttribute()) {
            if (!requestClassicScript(sourceAttributeValue()))
                return false;
            potentiallyBlockRendering();
        }
        break;
    }
    case ScriptType::Module: {
        if (!requestModuleScript(scriptStartPosition))
            return false;
        potentiallyBlockRendering();
        break;
    }
    case ScriptType::ImportMap: {
        // If the element’s node document's acquiring import maps is false, then queue a task to fire an event named error at the element, and return.
        RefPtr frame { element->document().frame() };
        if (!frame || !frame->script().isAcquiringImportMaps()) {
            element->document().eventLoop().queueTask(TaskSource::DOMManipulation, [this, element] {
                dispatchErrorEvent();
            });
            return false;
        }
        frame->script().setAcquiringImportMaps();
        if (hasSourceAttribute()) {
            if (!requestImportMap(*frame, sourceAttributeValue()))
                return false;
            potentiallyBlockRendering();
        } else
            frame->script().setPendingImportMaps();
        break;
    }
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
        document->checkedScriptRunner()->queueScriptForExecution(*this, *m_loadableScript, ScriptRunner::IN_ORDER_EXECUTION);
    } else if (hasSourceAttribute() || scriptType == ScriptType::Module) {
        ASSERT(m_loadableScript);
        ASSERT(hasAsyncAttribute() || m_forceAsync);
        document->checkedScriptRunner()->queueScriptForExecution(*this, *m_loadableScript, ScriptRunner::ASYNC_EXECUTION);
    } else if (!hasSourceAttribute() && m_parserInserted == ParserInserted::Yes && !document->haveStylesheetsLoaded()) {
        ASSERT(scriptType == ScriptType::Classic || scriptType == ScriptType::ImportMap);
        m_willBeParserExecuted = true;
        m_readyToBeParserExecuted = true;
    } else {
        ASSERT(scriptType == ScriptType::Classic || scriptType == ScriptType::ImportMap);
        TextPosition position = document->isInDocumentWrite() ? TextPosition() : scriptStartPosition;
        if (scriptType == ScriptType::Classic)
            executeClassicScript(ScriptSourceCode(sourceText, m_taintedOrigin, URL(document->url()), position, JSC::SourceProviderSourceType::Program, InlineClassicScript::create(*this)));
        else
            registerImportMap(ScriptSourceCode(sourceText, m_taintedOrigin, URL(document->url()), position, JSC::SourceProviderSourceType::ImportMap));
    }

    return true;
}

bool ScriptElement::requestClassicScript(const String& sourceURL)
{
    auto element = protectedElement();
    ASSERT(element->isConnected());
    ASSERT(!m_loadableScript);
    Ref document = element->document();
    if (!StringView(sourceURL).containsOnly<isASCIIWhitespace<UChar>>()) {
        auto script = LoadableClassicScript::create(element->nonce(), element->attributeWithoutSynchronization(HTMLNames::integrityAttr), referrerPolicy(), fetchPriorityHint(),
            element->attributeWithoutSynchronization(HTMLNames::crossoriginAttr), scriptCharset(), element->localName(), element->isInUserAgentShadowTree(), hasAsyncAttribute());

        auto scriptURL = document->completeURL(sourceURL);
        document->willLoadScriptElement(scriptURL);

        if (!document->checkedContentSecurityPolicy()->allowNonParserInsertedScripts(scriptURL, URL(), m_startLineNumber, element->nonce(), String(), m_parserInserted))
            return false;

        if (script->load(document, scriptURL)) {
            m_loadableScript = WTFMove(script);
            m_isExternalScript = true;
        }
    }

    if (m_loadableScript)
        return true;

    document->eventLoop().queueTask(TaskSource::DOMManipulation, [this, element = protectedElement()] {
        dispatchErrorEvent();
    });
    return false;
}

bool ScriptElement::requestModuleScript(const TextPosition& scriptStartPosition)
{
    // https://html.spec.whatwg.org/multipage/urls-and-fetching.html#cors-settings-attributes
    // Module is always CORS request. If attribute is not given, it should be same-origin credential.
    Ref element = this->element();
    Ref document = element->document();
    auto nonce = element->nonce();
    auto crossOriginMode = element->attributeWithoutSynchronization(HTMLNames::crossoriginAttr);
    if (crossOriginMode.isNull())
        crossOriginMode = ScriptElementCachedScriptFetcher::defaultCrossOriginModeForModule;

    if (hasSourceAttribute()) {
        ASSERT(element->isConnected());

        String sourceURL = sourceAttributeValue();
        if (StringView(sourceURL).containsOnly<isASCIIWhitespace<UChar>>()) {
            dispatchErrorEvent();
            return false;
        }

        auto moduleScriptRootURL = document->completeURL(sourceURL);
        if (!moduleScriptRootURL.isValid()) {
            dispatchErrorEvent();
            return false;
        }

        m_isExternalScript = true;
        AtomString integrity = element->attributeWithoutSynchronization(HTMLNames::integrityAttr);
        if (integrity.isNull())
            integrity = AtomString { document->globalObject()->importMap().integrityForURL(moduleScriptRootURL) };
        Ref script = LoadableModuleScript::create(nonce, integrity, referrerPolicy(), fetchPriorityHint(), crossOriginMode,
            scriptCharset(), element->localName(), element->isInUserAgentShadowTree());
        m_loadableScript = script.copyRef();
        if (RefPtr frame = element->document().frame())
            frame->checkedScript()->loadModuleScript(script, moduleScriptRootURL, script->parameters());
        return true;
    }

    Ref script = LoadableModuleScript::create(nonce, emptyAtom(), referrerPolicy(), fetchPriorityHint(), crossOriginMode, scriptCharset(), element->localName(), element->isInUserAgentShadowTree());

    TextPosition position = document->isInDocumentWrite() ? TextPosition() : scriptStartPosition;
    ScriptSourceCode sourceCode(scriptContent(), m_taintedOrigin, URL(document->url()), position, JSC::SourceProviderSourceType::Module, script.copyRef());

    ASSERT(document->contentSecurityPolicy());
    {
        CheckedRef contentSecurityPolicy = *document->contentSecurityPolicy();
        if (!contentSecurityPolicy->allowNonParserInsertedScripts(URL(), document->url(), m_startLineNumber, element->nonce(), sourceCode.source(), m_parserInserted))
            return false;

        if (!contentSecurityPolicy->allowInlineScript(document->url().string(), m_startLineNumber, sourceCode.source(), element, nonce, element->isInUserAgentShadowTree()))
            return false;
    }

    m_loadableScript = script.copyRef();
    if (RefPtr frame = document->frame())
        frame->checkedScript()->loadModuleScript(script, sourceCode);
    return true;
}

bool ScriptElement::requestImportMap(LocalFrame& frame, const String& sourceURL)
{
    Ref element = this->element();
    Ref document = element->document();

    ASSERT(element->isConnected());
    ASSERT(!m_loadableScript);
    if (!StringView(sourceURL).containsOnly<isASCIIWhitespace<UChar>>()) {
        Ref script = LoadableImportMap::create(element->nonce(), element->attributeWithoutSynchronization(HTMLNames::integrityAttr), referrerPolicy(),
            element->attributeWithoutSynchronization(HTMLNames::crossoriginAttr), element->localName(), element->isInUserAgentShadowTree(), hasAsyncAttribute());

        auto scriptURL = document->completeURL(sourceURL);
        document->willLoadScriptElement(scriptURL);

        if (!document->checkedContentSecurityPolicy()->allowNonParserInsertedScripts(scriptURL, URL(), m_startLineNumber, element->nonce(), String(), m_parserInserted))
            return false;

        frame.checkedScript()->setPendingImportMaps();
        if (script->load(document, scriptURL)) {
            m_loadableScript = WTFMove(script);
            m_isExternalScript = true;
        }
    }

    if (m_loadableScript)
        return true;

    document->checkedEventLoop()->queueTask(TaskSource::DOMManipulation, [this, element] {
        dispatchErrorEvent();
    });
    return false;
}

void ScriptElement::executeClassicScript(const ScriptSourceCode& sourceCode)
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isScriptAllowed());
    ASSERT(m_alreadyStarted);

    if (sourceCode.isEmpty())
        return;

    Ref element = this->element();
    Ref document = element->document();
    if (!m_isExternalScript) {
        ASSERT(document->contentSecurityPolicy());
        CheckedRef contentSecurityPolicy = *document->contentSecurityPolicy();
        if (!contentSecurityPolicy->allowNonParserInsertedScripts(URL(), document->url(), m_startLineNumber, element->nonce(), sourceCode.source(), m_parserInserted))
            return;

        if (!contentSecurityPolicy->allowInlineScript(document->url().string(), m_startLineNumber, sourceCode.source(), element, element->nonce(), element->isInUserAgentShadowTree()))
            return;
    }

    RefPtr frame = document->frame();
    if (!frame)
        return;

    IgnoreDestructiveWriteCountIncrementer ignoreDestructiveWriteCountIncrementer(m_isExternalScript ? document.ptr() : nullptr);
    CurrentScriptIncrementer currentScriptIncrementer(document, *this);

    WTFBeginSignpost(this, ExecuteScriptElement, "executing classic script from URL: %" PRIVATE_LOG_STRING " async: %d defer: %d", m_isExternalScript ? sourceCode.url().string().utf8().data() : "inline", hasAsyncAttribute(), hasDeferAttribute());
    frame->checkedScript()->evaluateIgnoringException(sourceCode);
    WTFEndSignpost(this, ExecuteScriptElement);
}

void ScriptElement::registerImportMap(const ScriptSourceCode& sourceCode)
{
    // https://wicg.github.io/import-maps/#integration-register-an-import-map

    ASSERT(m_alreadyStarted);
    ASSERT(scriptType() == ScriptType::ImportMap);

    Ref element = this->element();
    Ref document = element->document();
    RefPtr frame = document->frame();

    auto scopedExit = WTF::makeScopeExit([&] {
        if (frame)
            frame->checkedScript()->clearPendingImportMaps();
    });

    if (sourceCode.isEmpty()) {
        dispatchErrorEvent();
        return;
    }

    if (!m_isExternalScript) {
        ASSERT(document->contentSecurityPolicy());
        CheckedRef contentSecurityPolicy = *document->contentSecurityPolicy();
        if (!contentSecurityPolicy->allowNonParserInsertedScripts(URL(), document->url(), m_startLineNumber, element->nonce(), sourceCode.source(), m_parserInserted))
            return;

        if (!contentSecurityPolicy->allowInlineScript(document->url().string(), m_startLineNumber, sourceCode.source(), element, element->nonce(), element->isInUserAgentShadowTree()))
            return;
    }

    if (!frame)
        return;

    WTFBeginSignpost(this, RegisterImportMap, "registering import-map from URL: %" PRIVATE_LOG_STRING " async: %d defer: %d", m_isExternalScript ? sourceCode.url().string().utf8().data() : "inline", hasAsyncAttribute(), hasDeferAttribute());
    frame->checkedScript()->registerImportMap(sourceCode, document->baseURL());
    WTFEndSignpost(this, RegisterImportMap);
}

void ScriptElement::executeModuleScript(LoadableModuleScript& loadableModuleScript)
{
    // https://html.spec.whatwg.org/multipage/scripting.html#execute-the-script-block

    ASSERT(!loadableModuleScript.hasError());

    Ref document = element().document();
    RefPtr frame = document->frame();
    if (!frame)
        return;

    IgnoreDestructiveWriteCountIncrementer ignoreDestructiveWriteCountIncrementer(document.ptr());
    CurrentScriptIncrementer currentScriptIncrementer(document, *this);

    WTFBeginSignpost(this, ExecuteScriptElement, "executing module script");
    frame->script().linkAndEvaluateModuleScript(loadableModuleScript);
    WTFEndSignpost(this, ExecuteScriptElement, "executing module script");
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
    if (auto error = loadableScript.takeError()) {
        // There are three types of errors in script loading, fetch error, parse error, and script error.
        // 1. Fetch error dispatches an error event on <script> tag, but not on window.
        // 2. Parse error dispatches an error event on window, but not on <script>. And
        //    it still dispatches a load event on <script>.
        // 3. Script error dispatches an error event on window.
        switch (error->type) {
        // Fetch error
        case LoadableScript::ErrorType::Fetch:
        case LoadableScript::ErrorType::CrossOriginLoad:
        case LoadableScript::ErrorType::MIMEType:
        case LoadableScript::ErrorType::Nosniff:
        case LoadableScript::ErrorType::FailedIntegrityCheck: {
            // https://html.spec.whatwg.org/multipage/scripting.html#execute-the-script-block
            // When the script is "null" due to a fetch error, an error event
            // should be dispatched for the script element.
            if (std::optional<LoadableScript::ConsoleMessage> message = error->consoleMessage)
                element().document().addConsoleMessage(message->source, message->level, message->message);
            dispatchErrorEvent();
            break;
        }

        // Parse error
        case LoadableScript::ErrorType::Resolve: {
            if (RefPtr frame = element().document().frame())
                frame->checkedScript()->reportExceptionFromScriptError(error.value(), loadableScript.isModuleScript());
            dispatchLoadEventRespectingUserGestureIndicator();
            break;
        }

        // Script error
        case LoadableScript::ErrorType::Script: {
            // https://html.spec.whatwg.org/multipage/webappapis.html#report-the-exception
            // An error value is present when there is a load failure that was
            // not triggered during fetching. In this case, we need to report
            // the exception to the global object.
            if (RefPtr frame = element().document().frame())
                frame->checkedScript()->reportExceptionFromScriptError(error.value(), loadableScript.isModuleScript());
            break;
        }
        }
    } else if (!loadableScript.wasCanceled()) {
        loadableScript.execute(*this);
        dispatchLoadEventRespectingUserGestureIndicator();
    }
}

void ScriptElement::executePendingScript(PendingScript& pendingScript)
{
    unblockRendering();
    auto* loadableScript = pendingScript.loadableScript();
    RefPtr<Document> document { &element().document() };
    if (document->identifier() != m_preparationTimeDocumentIdentifier) {
        document->addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Not executing script because it moved between documents during fetching"_s);
        if (loadableScript) {
            if (auto* loadableImportMap = dynamicDowncast<LoadableImportMap>(loadableScript))
                document = loadableImportMap->document();
        }
    } else {
        if (loadableScript)
            executeScriptAndDispatchEvent(*loadableScript);
        else {
            ASSERT(!pendingScript.hasError());
            ASSERT_WITH_MESSAGE(scriptType() == ScriptType::Classic || scriptType() == ScriptType::ImportMap, "Module script always have a loadableScript pointer.");
            if (scriptType() == ScriptType::Classic)
                executeClassicScript(ScriptSourceCode(scriptContent(), m_taintedOrigin, URL(element().document().url()), pendingScript.startingPosition(), JSC::SourceProviderSourceType::Program, InlineClassicScript::create(*this)));
            else
                registerImportMap(ScriptSourceCode(scriptContent(), m_taintedOrigin, URL(element().document().url()), pendingScript.startingPosition(), JSC::SourceProviderSourceType::ImportMap));
            dispatchLoadEventRespectingUserGestureIndicator();
        }
    }

    if (scriptType() == ScriptType::ImportMap && document) {
        if (RefPtr frame = document->frame())
            frame->checkedScript()->clearPendingImportMaps();
    }
}

bool ScriptElement::ignoresLoadRequest() const
{
    return m_alreadyStarted || m_isExternalScript || m_parserInserted == ParserInserted::Yes || !element().isConnected();
}

String ScriptElement::scriptContent() const
{
    return TextNodeTraversal::childTextContent(protectedElement());
}

void ScriptElement::setTrustedScriptText(const String& text)
{
    m_trustedScriptText = text;
}

void ScriptElement::ref() const
{
    element().ref();
}

void ScriptElement::deref() const
{
    element().deref();
}

bool isScriptElement(Element& element)
{
    return is<HTMLScriptElement>(element) || is<SVGScriptElement>(element);
}

ScriptElement* dynamicDowncastScriptElement(Element& element)
{
    if (auto* htmlElement = dynamicDowncast<HTMLScriptElement>(element))
        return htmlElement;
    return dynamicDowncast<SVGScriptElement>(element);
}

}
