/*
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2019 Apple Inc. All Rights Reserved.
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
#include "ContentSecurityPolicy.h"
#include "Element.h"
#include "Frame.h"
#include "JSNode.h"
#include "QualifiedName.h"
#include "SVGElement.h"
#include "ScriptController.h"
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/FunctionConstructor.h>
#include <JavaScriptCore/IdentifierInlines.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
using namespace JSC;

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, eventListenerCounter, ("JSLazyEventListener"));

struct JSLazyEventListener::CreationArguments {
    const QualifiedName& attributeName;
    const AtomString& attributeValue;
    Document& document;
    WeakPtr<ContainerNode> node;
    JSObject* wrapper;
    bool shouldUseSVGEventName;
};

static const String& eventParameterName(bool shouldUseSVGEventName)
{
    static NeverDestroyed<const String> eventString(MAKE_STATIC_STRING_IMPL("event"));
    static NeverDestroyed<const String> evtString(MAKE_STATIC_STRING_IMPL("evt"));
    return shouldUseSVGEventName ? evtString : eventString;
}

static TextPosition convertZeroToOne(const TextPosition& position)
{
    // A JSLazyEventListener can be created with a line number of zero when it is created with
    // a setAttribute call from JavaScript, so make the line number 1 in that case.
    if (position == TextPosition::belowRangePosition())
        return { };
    return position;
}

JSLazyEventListener::JSLazyEventListener(CreationArguments&& arguments, const String& sourceURL, const TextPosition& sourcePosition)
    : JSEventListener(nullptr, arguments.wrapper, true, mainThreadNormalWorld())
    , m_functionName(arguments.attributeName.localName().string())
    , m_eventParameterName(eventParameterName(arguments.shouldUseSVGEventName))
    , m_code(arguments.attributeValue)
    , m_sourceURL(sourceURL)
    , m_sourcePosition(convertZeroToOne(sourcePosition))
    , m_originalNode(WTFMove(arguments.node))
{
#ifndef NDEBUG
    eventListenerCounter.increment();
#endif
}

#if ASSERT_ENABLED
static inline bool isCloneInShadowTreeOfSVGUseElement(Node& originalNode, EventTarget& eventTarget)
{
    if (!eventTarget.isNode())
        return false;

    auto& node = downcast<Node>(eventTarget);
    if (!is<SVGElement>(node))
        return false;

    auto& element = downcast<SVGElement>(node);
    if (!element.correspondingElement())
        return false;

    ASSERT(element.isInShadowTree());
    return &originalNode == element.correspondingElement();
}

// This is to help find the underlying cause of <rdar://problem/24314027>.
void JSLazyEventListener::checkValidityForEventTarget(EventTarget& eventTarget)
{
    if (eventTarget.isNode()) {
        ASSERT(m_originalNode);
        ASSERT(static_cast<EventTarget*>(m_originalNode.get()) == &eventTarget || isCloneInShadowTreeOfSVGUseElement(*m_originalNode, eventTarget));
    } else
        ASSERT(!m_originalNode);
}
#endif

JSLazyEventListener::~JSLazyEventListener()
{
#ifndef NDEBUG
    eventListenerCounter.decrement();
#endif
}

JSObject* JSLazyEventListener::initializeJSFunction(ScriptExecutionContext& executionContext) const
{
    ASSERT(is<Document>(executionContext));

    auto& executionContextDocument = downcast<Document>(executionContext);

    // As per the HTML specification [1], if this is an element's event handler, then document should be the
    // element's document. The script execution context may be different from the node's document if the
    // node's document was created by JavaScript.
    // [1] https://html.spec.whatwg.org/multipage/webappapis.html#getting-the-current-value-of-the-event-handler
    auto& document = m_originalNode ? m_originalNode->document() : executionContextDocument;
    if (!document.frame())
        return nullptr;

    if (!document.contentSecurityPolicy()->allowInlineEventHandlers(m_sourceURL, m_sourcePosition.m_line))
        return nullptr;

    auto& script = document.frame()->script();
    if (!script.canExecuteScripts(AboutToCreateEventListener) || script.isPaused())
        return nullptr;

    if (!executionContextDocument.frame())
        return nullptr;
    auto* globalObject = toJSDOMWindow(*executionContextDocument.frame(), isolatedWorld());
    if (!globalObject)
        return nullptr;

    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSGlobalObject* lexicalGlobalObject = globalObject;

    MarkedArgumentBuffer args;
    args.append(jsNontrivialString(vm, m_eventParameterName));
    args.append(jsStringWithCache(lexicalGlobalObject, m_code));
    ASSERT(!args.hasOverflowed());

    // We want all errors to refer back to the line on which our attribute was
    // declared, regardless of any newlines in our JavaScript source text.
    int overrideLineNumber = m_sourcePosition.m_line.oneBasedInt();

    JSObject* jsFunction = constructFunctionSkippingEvalEnabledCheck(
        lexicalGlobalObject, args, Identifier::fromString(vm, m_functionName),
        SourceOrigin { m_sourceURL, CachedScriptFetcher::create(document.charset()) },
        m_sourceURL, m_sourcePosition, overrideLineNumber);
    if (UNLIKELY(scope.exception())) {
        reportCurrentException(lexicalGlobalObject);
        scope.clearException();
        return nullptr;
    }

    JSFunction* listenerAsFunction = jsCast<JSFunction*>(jsFunction);

    if (m_originalNode) {
        if (!wrapper()) {
            // Ensure that 'node' has a JavaScript wrapper to mark the event listener we're creating.
            // FIXME: Should pass the global object associated with the node
            setWrapper(vm, asObject(toJS(lexicalGlobalObject, globalObject, *m_originalNode)));
        }

        // Add the event's home element to the scope
        // (and the document, and the form - see JSHTMLElement::eventHandlerScope)
        listenerAsFunction->setScope(vm, jsCast<JSNode*>(wrapper())->pushEventHandlerScope(lexicalGlobalObject, listenerAsFunction->scope()));
    }

    return jsFunction;
}

RefPtr<JSLazyEventListener> JSLazyEventListener::create(CreationArguments&& arguments)
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

    return adoptRef(*new JSLazyEventListener(WTFMove(arguments), sourceURL, position));
}

RefPtr<JSLazyEventListener> JSLazyEventListener::create(Element& element, const QualifiedName& attributeName, const AtomString& attributeValue)
{
    return create({ attributeName, attributeValue, element.document(), makeWeakPtr(element), nullptr, element.isSVGElement() });
}

RefPtr<JSLazyEventListener> JSLazyEventListener::create(Document& document, const QualifiedName& attributeName, const AtomString& attributeValue)
{
    // FIXME: This always passes false for "shouldUseSVGEventName". Is that correct for events dispatched to SVG documents?
    // This has been this way for a long time, but became more obvious when refactoring to separate the Element and Document code paths.
    return create({ attributeName, attributeValue, document, makeWeakPtr(document), nullptr, false });
}

RefPtr<JSLazyEventListener> JSLazyEventListener::create(DOMWindow& window, const QualifiedName& attributeName, const AtomString& attributeValue)
{
    ASSERT(window.document());
    auto& document = *window.document();
    ASSERT(document.frame());
    return create({ attributeName, attributeValue, document, nullptr, toJSDOMWindow(document.frame(), mainThreadNormalWorld()), document.isSVGDocument() });
}

} // namespace WebCore
