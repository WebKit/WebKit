/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
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
#include "DOMWrapperWorld.h"
#include "DocumentEventLoop.h"
#include "DocumentInlines.h"
#include "DocumentPage.h"
#include "ElementInlines.h"
#include "Event.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "HTMLAnchorElement.h"
#include "HTMLCollection.h"
#include "HTMLNames.h"
#include "HTMLScriptElement.h"
#include "IgnoreDestructiveWriteCountIncrementer.h"
#include "InlineClassicScript.h"
#include "LoadableClassicScript.h"
#include "LoadableModuleScript.h"
#include "LoadableScriptError.h"
#include "LocalFrame.h"
#include "MIMETypeRegistry.h"
#include "ModuleFetchParameters.h"
#include "PendingScript.h"
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
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/ImportMap.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSLock.h>
#include <wtf/RuntimeApplicationChecks.h>
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

static void reportSpeculationRulesError(LocalFrame& frame, const String& errorMessage)
{
    // https://html.spec.whatwg.org/C#report-an-exception
    auto& world = mainThreadNormalWorldSingleton();
    JSC::VM& vm = world.vm();
    JSLockHolder lock(vm);

    if (auto* jsGlobalObject = frame.checkedScript()->globalObject(world)) {
        auto* error = JSC::createTypeError(jsGlobalObject, errorMessage);
        LoadableScript::Error scriptError {
            LoadableScriptErrorType::Script,
            std::nullopt,
            JSC::Strong<JSC::Unknown>(vm, error)
        };
        frame.checkedScript()->reportExceptionFromScriptError(scriptError, false);
    }
}

// https://html.spec.whatwg.org/C#prepare-the-script-element (Steps 8-12)
std::optional<ScriptType> ScriptElement::determineScriptType(const String& type, const String& language, bool isHTMLDocument, bool speculationRulesPrefetchEnabled)
{
    // Step 8. If any of the following are true:
    //  - el has a type attribute whose value is the empty string;
    //  - el has no type attribute but it has a language attribute and that attribute's value is the empty string; or
    //  - el has neither a type attribute nor a language attribute,
    // then let the script block's type string for this script element be "text/javascript".
    if (type.isNull()) {
        if (language.isEmpty())
            return ScriptType::Classic;
        if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(makeString("text/"_s, language)))
            return ScriptType::Classic;
        return std::nullopt;
    }
    if (type.isEmpty())
        return ScriptType::Classic; // Assume text/javascript.

    // Step 9. If the script block's type string is a JavaScript MIME type essence match, then set el's type to "classic".
    if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(type.trim(isASCIIWhitespace)))
        return ScriptType::Classic;

    // FIXME: XHTML spec defines "defer" attribute. But WebKit does not implement it for a long time.
    // And module tag also uses defer attribute semantics. We disable script type="module" for non HTML document.
    // Once "defer" is implemented, we can reconsider enabling modules in XHTML.
    // https://bugs.webkit.org/show_bug.cgi?id=123387
    if (!isHTMLDocument)
        return std::nullopt;

    // Step 10. Otherwise, if the script block's type string is an ASCII case-insensitive match for the string "module", then set el's type to "module".
    if (equalLettersIgnoringASCIICase(type, "module"_s))
        return ScriptType::Module;

    // Step 11. Otherwise, if the script block's type string is an ASCII case-insensitive match for the string "importmap", then set el's type to "importmap".
    if (equalLettersIgnoringASCIICase(type, "importmap"_s))
        return ScriptType::ImportMap;

    // Step 12. Otherwise, if the script block's type string is an ASCII case-insensitive match for the string "speculationrules", then set el's type to "speculationrules".
    if (speculationRulesPrefetchEnabled && equalLettersIgnoringASCIICase(type, "speculationrules"_s))
        return ScriptType::SpeculationRules;

    return std::nullopt;
}

std::optional<ScriptType> ScriptElement::determineScriptType() const
{
    return determineScriptType(typeAttributeValue(), languageAttributeValue(), element().document().isHTMLDocument(), element().document().settings().speculationRulesPrefetchEnabled());
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
    Ref element = this->element();
    Ref context = *element->scriptExecutionContext();
    if (context->settingsValues().trustedTypesEnabled && sourceText != m_trustedScriptText) {
        auto trustedText = trustedTypeCompliantString(TrustedType::TrustedScript, context, sourceText, is<HTMLScriptElement>(element) ? "HTMLScriptElement text"_s : "SVGScriptElement text"_s);
        if (trustedText.hasException())
            return false;
        sourceText = trustedText.releaseReturnValue();
    }

    if (!hasSourceAttribute() && sourceText.isEmpty())
        return false;

    if (!element->isConnected())
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
    if (auto attributeValue = charsetAttributeValue(); !attributeValue.isEmpty())
        m_characterEncoding = WTFMove(attributeValue);
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
        if (!requestModuleScript(sourceText, scriptStartPosition))
            return false;
        potentiallyBlockRendering();
        break;
    }
    case ScriptType::ImportMap:
    case ScriptType::SpeculationRules: {
        // If the element has a source attribute, queue a task to fire an event named error at the element, and return.
        if (hasSourceAttribute()) {
            element->protectedDocument()->checkedEventLoop()->queueTask(TaskSource::DOMManipulation, [protectedThis = Ref { *this }] {
                protectedThis->dispatchErrorEvent();
            });
            return false;
        }
        break;
    }
    }

    updateTaintedOriginFromSourceURL();

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
        document->protectedScriptRunner()->queueScriptForExecution(*this, *protectedLoadableScript(), ScriptRunner::IN_ORDER_EXECUTION);
    } else if (hasSourceAttribute() || scriptType == ScriptType::Module) {
        ASSERT(m_loadableScript);
        ASSERT(hasAsyncAttribute() || m_forceAsync);
        document->protectedScriptRunner()->queueScriptForExecution(*this, *protectedLoadableScript(), ScriptRunner::ASYNC_EXECUTION);
    } else if (!hasSourceAttribute() && m_parserInserted == ParserInserted::Yes && !document->haveStylesheetsLoaded()) {
        ASSERT(scriptType == ScriptType::Classic || scriptType == ScriptType::ImportMap || scriptType == ScriptType::SpeculationRules);
        m_willBeParserExecuted = true;
        m_readyToBeParserExecuted = true;
    } else {
        ASSERT(scriptType == ScriptType::Classic || scriptType == ScriptType::ImportMap || scriptType == ScriptType::SpeculationRules);
        TextPosition position = document->isInDocumentWrite() ? TextPosition() : scriptStartPosition;
        if (scriptType == ScriptType::Classic)
            executeClassicScript(ScriptSourceCode(sourceText, m_taintedOrigin, URL(document->url()), position, JSC::SourceProviderSourceType::Program, InlineClassicScript::create(*this)));
        else if (scriptType == ScriptType::ImportMap)
            registerImportMap(ScriptSourceCode(sourceText, m_taintedOrigin, URL(document->url()), position, JSC::SourceProviderSourceType::ImportMap));
        else
            registerSpeculationRules(ScriptSourceCode(sourceText, m_taintedOrigin, URL(document->url()), position, JSC::SourceProviderSourceType::Program));
    }

    return true;
}

void ScriptElement::updateTaintedOriginFromSourceURL()
{
    if (m_taintedOrigin == JSC::SourceTaintedOrigin::KnownTainted)
        return;

    Ref document = element().document();
    RefPtr page = document->page();
    if (!page)
        return;

    if (!page->requiresScriptTrackingPrivacyProtections(hasSourceAttribute() ? document->completeURL(sourceAttributeValue()) : document->url()))
        return;

    m_taintedOrigin = JSC::SourceTaintedOrigin::KnownTainted;
}

bool ScriptElement::requestClassicScript(const String& sourceURL)
{
    Ref element = this->element();
    ASSERT(element->isConnected());
    ASSERT(!m_loadableScript);
    Ref document = element->document();
    if (!StringView(sourceURL).containsOnly<isASCIIWhitespace<char16_t>>()) {
        auto script = LoadableClassicScript::create(element->nonce(), element->attributeWithoutSynchronization(HTMLNames::integrityAttr), referrerPolicy(), fetchPriority(),
            element->attributeWithoutSynchronization(HTMLNames::crossoriginAttr), scriptCharset(), element->localName(), element->isInUserAgentShadowTree(), hasAsyncAttribute());

        auto scriptURL = document->completeURL(sourceURL);
        document->willLoadScriptElement(scriptURL);

        if (!document->checkedContentSecurityPolicy()->allowNonParserInsertedScripts(scriptURL, URL(), m_startLineNumber, element->nonce(), script->integrity(), String(), m_parserInserted))
            return false;

        if (script->load(document, scriptURL)) {
            m_loadableScript = WTFMove(script);
            m_isExternalScript = true;
        }
    }

    if (m_loadableScript)
        return true;

    document->eventLoop().queueTask(TaskSource::DOMManipulation, [protectedThis = Ref { *this }] {
        protectedThis->dispatchErrorEvent();
    });
    return false;
}

bool ScriptElement::requestModuleScript(const String& sourceText, const TextPosition& scriptStartPosition)
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
        if (StringView(sourceURL).containsOnly<isASCIIWhitespace<char16_t>>()) {
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
        Ref script = LoadableModuleScript::create(LoadableModuleScript::IsInline::No, nonce, integrity, referrerPolicy(), fetchPriority(), crossOriginMode,
            scriptCharset(), element->localName(), element->isInUserAgentShadowTree());
        m_loadableScript = script.copyRef();
        if (RefPtr frame = element->document().frame())
            frame->checkedScript()->loadModuleScript(script, moduleScriptRootURL, script->parameters());
        return true;
    }

    Ref script = LoadableModuleScript::create(LoadableModuleScript::IsInline::Yes, nonce, emptyAtom(), referrerPolicy(), fetchPriority(), crossOriginMode, scriptCharset(), element->localName(), element->isInUserAgentShadowTree());

    TextPosition position = document->isInDocumentWrite() ? TextPosition() : scriptStartPosition;
    ScriptSourceCode sourceCode(sourceText, m_taintedOrigin, URL(document->url()), position, JSC::SourceProviderSourceType::Module, script.copyRef());

    ASSERT(document->contentSecurityPolicy());
    {
        CheckedRef contentSecurityPolicy = *document->contentSecurityPolicy();
        if (!contentSecurityPolicy->allowNonParserInsertedScripts(URL(), document->url(), m_startLineNumber, element->nonce(), script->parameters().integrity(), sourceCode.source(), m_parserInserted))
            return false;

        if (!contentSecurityPolicy->allowInlineScript(document->url().string(), m_startLineNumber, sourceCode.source(), element, nonce, element->isInUserAgentShadowTree()))
            return false;
    }

    m_loadableScript = script.copyRef();
    if (RefPtr frame = document->frame())
        frame->checkedScript()->loadModuleScript(script, sourceCode);
    return true;
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
        if (!contentSecurityPolicy->allowNonParserInsertedScripts(URL(), document->url(), m_startLineNumber, element->nonce(), emptyString(), sourceCode.source(), m_parserInserted))
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
    // https://html.spec.whatwg.org/#register-an-import-map

    ASSERT(m_alreadyStarted);
    ASSERT(scriptType() == ScriptType::ImportMap);

    Ref element = this->element();
    Ref document = element->document();
    RefPtr frame = document->frame();

    if (sourceCode.isEmpty()) {
        dispatchErrorEvent();
        return;
    }

    if (!m_isExternalScript) {
        ASSERT(document->contentSecurityPolicy());
        CheckedRef contentSecurityPolicy = *document->contentSecurityPolicy();
        if (!contentSecurityPolicy->allowNonParserInsertedScripts(URL(), document->url(), m_startLineNumber, element->nonce(), emptyString(), sourceCode.source(), m_parserInserted))
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
                element().protectedDocument()->addConsoleMessage(message->source, message->level, message->message);
            dispatchErrorEvent();
            break;
        }

        // Parse error
        case LoadableScript::ErrorType::Resolve: {
            if (RefPtr frame = element().document().frame())
                frame->checkedScript()->reportExceptionFromScriptError(error.value(), loadableScript.isModuleScript());
            if (!loadableScript.isInlineModule())
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
        if (!loadableScript.isInlineModule())
            dispatchLoadEventRespectingUserGestureIndicator();
    }
}

void ScriptElement::executePendingScript(PendingScript& pendingScript)
{
    unblockRendering();
    RefPtr loadableScript = pendingScript.loadableScript();
    Ref document = element().document();
    if (document->identifier() != m_preparationTimeDocumentIdentifier) {
        document->addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Not executing script because it moved between documents during fetching"_s);
    } else {
        if (loadableScript)
            executeScriptAndDispatchEvent(*loadableScript);
        else {
            ASSERT(!pendingScript.hasError());
            ASSERT_WITH_MESSAGE(scriptType() == ScriptType::Classic || scriptType() == ScriptType::ImportMap || (scriptType() == ScriptType::SpeculationRules && document->settings().speculationRulesPrefetchEnabled()), "Module script always have a loadableScript pointer.");
            if (scriptType() == ScriptType::Classic)
                executeClassicScript(ScriptSourceCode(scriptContent(), m_taintedOrigin, URL(document->url()), pendingScript.startingPosition(), JSC::SourceProviderSourceType::Program, InlineClassicScript::create(*this)));
            else if (scriptType() == ScriptType::ImportMap)
                registerImportMap(ScriptSourceCode(scriptContent(), m_taintedOrigin, URL(document->url()), pendingScript.startingPosition(), JSC::SourceProviderSourceType::ImportMap));
            else
                registerSpeculationRules(ScriptSourceCode(scriptContent(), m_taintedOrigin, URL(document->url()), pendingScript.startingPosition(), JSC::SourceProviderSourceType::Program));
            dispatchLoadEventRespectingUserGestureIndicator();
        }
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

bool isScriptElement(Node& node)
{
    return is<HTMLScriptElement>(node) || is<SVGScriptElement>(node);
}

ScriptElement* dynamicDowncastScriptElement(Element& element)
{
    if (auto* htmlElement = dynamicDowncast<HTMLScriptElement>(element))
        return htmlElement;
    return dynamicDowncast<SVGScriptElement>(element);
}

// https://html.spec.whatwg.org/C#register-speculation-rules
void ScriptElement::registerSpeculationRules(const ScriptSourceCode& sourceCode)
{
    ASSERT(m_alreadyStarted);
    ASSERT(scriptType() == ScriptType::SpeculationRules);

    Ref element = this->element();
    Ref document = element->document();

    if (!document->settings().speculationRulesPrefetchEnabled())
        return;

    RefPtr frame = document->frame();

    if (sourceCode.isEmpty()) {
        dispatchErrorEvent();
        if (frame)
            reportSpeculationRulesError(*frame, "Speculation rules script has empty content"_s);
        return;
    }

    if (!m_isExternalScript) {
        CheckedPtr contentSecurityPolicy = document->contentSecurityPolicy();
        if (!contentSecurityPolicy)
            return;

        if (!contentSecurityPolicy->allowNonParserInsertedScripts(URL(), document->url(), m_startLineNumber, element->nonce(), emptyString(), sourceCode.source(), m_parserInserted))
            return;

        if (!contentSecurityPolicy->allowInlineScript(document->url().string(), m_startLineNumber, sourceCode.source(), element, element->nonce(), element->isInUserAgentShadowTree()))
            return;
    }

    if (!frame)
        return;

    if (frame->checkedScript()->registerSpeculationRules(sourceCode, document->baseURL()))
        document->considerSpeculationRules();
    else {
        dispatchErrorEvent();
        reportSpeculationRulesError(*frame, "Failed to register speculation rules"_s);
    }
}

// TODO: Also implement unregister/update speculation rules
// https://whatpr.org/html/11426/c9c4d33...28571ea/scripting.html#:~:text=The%20script%20%20HTML%20element,result%20%20given%20%20changedNode%20.

}
