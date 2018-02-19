/*
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2017 Apple Inc. All Rights Reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "JSLazyEventListener.h"

#include "CachedScriptFetcher.h"
#include "ContainerNode.h"
#include "ContentSecurityPolicy.h"
#include "Document.h"
#include "Element.h"
#include "Frame.h"
#include "JSNode.h"
#include "QualifiedName.h"
#include "ScriptController.h"
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/FunctionConstructor.h>
#include <JavaScriptCore/IdentifierInlines.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>


namespace WebCore {
using namespace JSC;

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, eventListenerCounter, ("JSLazyEventListener"));

JSLazyEventListener::JSLazyEventListener(const String& functionName, const String& eventParameterName, const String& code, ContainerNode* node, const String& sourceURL, const TextPosition& sourcePosition, JSObject* wrapper, DOMWrapperWorld& isolatedWorld)
    : JSEventListener(0, wrapper, true, isolatedWorld)
    , m_functionName(functionName)
    , m_eventParameterName(eventParameterName)
    , m_code(code)
    , m_sourceURL(sourceURL)
    , m_sourcePosition(sourcePosition)
    , m_originalNode(node)
{
    // We don't retain the original node because we assume it
    // will stay alive as long as this handler object is around
    // and we need to avoid a reference cycle. If JS transfers
    // this handler to another node, initializeJSFunction will
    // be called and then originalNode is no longer needed.

    // A JSLazyEventListener can be created with a line number of zero when it is created with
    // a setAttribute call from JavaScript, so make the line number 1 in that case.
    if (m_sourcePosition == TextPosition::belowRangePosition())
        m_sourcePosition = TextPosition();

    ASSERT(m_eventParameterName == "evt" || m_eventParameterName == "event");

#ifndef NDEBUG
    eventListenerCounter.increment();
#endif
}

JSLazyEventListener::~JSLazyEventListener()
{
#ifndef NDEBUG
    eventListenerCounter.decrement();
#endif
}

JSObject* JSLazyEventListener::initializeJSFunction(ScriptExecutionContext& executionContext) const
{
    ASSERT(is<Document>(executionContext));
    ASSERT(!m_code.isNull());
    ASSERT(!m_eventParameterName.isNull());
    if (m_code.isNull() || m_eventParameterName.isNull())
        return nullptr;

    // As per the HTML specification [1], if this is an element's event handler, then document should be the
    // element's document. The script execution context may be different from the node's document if the
    // node's document was created by JavaScript.
    // [1] https://html.spec.whatwg.org/multipage/webappapis.html#getting-the-current-value-of-the-event-handler
    Document& document = m_originalNode ? m_originalNode->document() : downcast<Document>(executionContext);

    if (!document.frame())
        return nullptr;

    if (!document.contentSecurityPolicy()->allowInlineEventHandlers(m_sourceURL, m_sourcePosition.m_line))
        return nullptr;

    ScriptController& script = document.frame()->script();
    if (!script.canExecuteScripts(AboutToCreateEventListener) || script.isPaused())
        return nullptr;

    JSDOMGlobalObject* globalObject = toJSDOMGlobalObject(&executionContext, isolatedWorld());
    if (!globalObject)
        return nullptr;

    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    ExecState* exec = globalObject->globalExec();

    MarkedArgumentBuffer args;
    args.append(jsNontrivialString(exec, m_eventParameterName));
    args.append(jsStringWithCache(exec, m_code));
    ASSERT(!args.hasOverflowed());

    // We want all errors to refer back to the line on which our attribute was
    // declared, regardless of any newlines in our JavaScript source text.
    int overrideLineNumber = m_sourcePosition.m_line.oneBasedInt();

    JSObject* jsFunction = constructFunctionSkippingEvalEnabledCheck(
        exec, exec->lexicalGlobalObject(), args, Identifier::fromString(exec, m_functionName),
        SourceOrigin { m_sourceURL, CachedScriptFetcher::create(document.charset()) }, m_sourceURL, m_sourcePosition, overrideLineNumber);

    if (UNLIKELY(scope.exception())) {
        reportCurrentException(exec);
        scope.clearException();
        return nullptr;
    }

    JSFunction* listenerAsFunction = jsCast<JSFunction*>(jsFunction);

    if (m_originalNode) {
        if (!wrapper()) {
            // Ensure that 'node' has a JavaScript wrapper to mark the event listener we're creating.
            // FIXME: Should pass the global object associated with the node
            setWrapper(vm, asObject(toJS(exec, globalObject, *m_originalNode)));
        }

        // Add the event's home element to the scope
        // (and the document, and the form - see JSHTMLElement::eventHandlerScope)
        listenerAsFunction->setScope(vm, jsCast<JSNode*>(wrapper())->pushEventHandlerScope(exec, listenerAsFunction->scope()));
    }
    return jsFunction;
}

static const String& eventParameterName(bool isSVGEvent)
{
    static NeverDestroyed<const String> eventString(MAKE_STATIC_STRING_IMPL("event"));
    static NeverDestroyed<const String> evtString(MAKE_STATIC_STRING_IMPL("evt"));
    return isSVGEvent ? evtString : eventString;
}

struct JSLazyEventListener::CreationArguments {
    const QualifiedName& attributeName;
    const AtomicString& attributeValue;
    Document& document;
    ContainerNode* node;
    JSC::JSObject* wrapper;
    bool shouldUseSVGEventName;
};

RefPtr<JSLazyEventListener> JSLazyEventListener::create(const CreationArguments& arguments)
{
    if (arguments.attributeValue.isNull())
        return nullptr;

    // FIXME: We should be able to provide source information for frameless documents too (e.g. for importing nodes from XMLHttpRequest.responseXML).
    TextPosition position;
    String sourceURL;
    if (Frame* frame = arguments.document.frame()) {
        if (!frame->script().canExecuteScripts(AboutToCreateEventListener))
            return nullptr;
        position = frame->script().eventHandlerPosition();
        sourceURL = arguments.document.url().string();
    }

    return adoptRef(*new JSLazyEventListener(arguments.attributeName.localName().string(),
        eventParameterName(arguments.shouldUseSVGEventName), arguments.attributeValue,
        arguments.node, sourceURL, position, arguments.wrapper, mainThreadNormalWorld()));
}

RefPtr<JSLazyEventListener> JSLazyEventListener::create(Element& element, const QualifiedName& attributeName, const AtomicString& attributeValue)
{
    return create({ attributeName, attributeValue, element.document(), &element, nullptr, element.isSVGElement() });
}

RefPtr<JSLazyEventListener> JSLazyEventListener::create(Document& document, const QualifiedName& attributeName, const AtomicString& attributeValue)
{
    // FIXME: This always passes false for "shouldUseSVGEventName". Is that correct for events dispatched to SVG documents?
    // This has been this way for a long time, but became more obvious when refactoring to separate the Element and Document code paths.
    return create({ attributeName, attributeValue, document, &document, nullptr, false });
}

RefPtr<JSLazyEventListener> JSLazyEventListener::create(DOMWindow& window, const QualifiedName& attributeName, const AtomicString& attributeValue)
{
    ASSERT(window.document());
    auto& document = *window.document();
    ASSERT(document.frame());
    return create({ attributeName, attributeValue, document, nullptr, toJSDOMWindow(document.frame(), mainThreadNormalWorld()), document.isSVGDocument() });
}

} // namespace WebCore
