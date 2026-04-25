/*
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2018 Apple Inc. All rights reserved.
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

#pragma once

#include "JSEventListener.h"
#include <wtf/Forward.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class ContainerNode;
class Document;
class Element;
class LocalDOMWindow;
class QualifiedName;

class JSLazyEventListener final : public JSEventListener {
public:
    static RefPtr<JSLazyEventListener> create(Element&, const QualifiedName& attributeName, const AtomString& attributeValue);
    static RefPtr<JSLazyEventListener> create(Document&, const QualifiedName& attributeName, const AtomString& attributeValue);
    static RefPtr<JSLazyEventListener> create(LocalDOMWindow&, const QualifiedName& attributeName, const AtomString& attributeValue);

    virtual ~JSLazyEventListener();

    URL sourceURL() const final { return m_sourceURL; }
    TextPosition sourcePosition() const final { return m_sourcePosition; }

private:
    struct CreationArguments;
    static RefPtr<JSLazyEventListener> create(CreationArguments&&);
    JSLazyEventListener(CreationArguments&&, const URL& sourceURL, const TextPosition&);
    String code() const final { return m_code; }

#if ASSERT_ENABLED
    void checkValidityForEventTarget(EventTarget&) final;
#endif

    JSC::JSObject* initializeJSFunction(ScriptExecutionContext&) const final;

    String m_functionName;
    const String& m_functionParameters;
    String m_code;
    URL m_sourceURL;
    TextPosition m_sourcePosition;
    WeakPtr<ContainerNode, WeakPtrImplWithEventTargetData> m_originalNode;
    JSC::SourceTaintedOrigin m_sourceTaintedOrigin;
};

} // namespace WebCore
