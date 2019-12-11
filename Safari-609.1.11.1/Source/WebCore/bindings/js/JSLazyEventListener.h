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
class DOMWindow;
class Document;
class Element;
class QualifiedName;

class JSLazyEventListener final : public JSEventListener {
public:
    static RefPtr<JSLazyEventListener> create(Element&, const QualifiedName& attributeName, const AtomString& attributeValue);
    static RefPtr<JSLazyEventListener> create(Document&, const QualifiedName& attributeName, const AtomString& attributeValue);
    static RefPtr<JSLazyEventListener> create(DOMWindow&, const QualifiedName& attributeName, const AtomString& attributeValue);

    virtual ~JSLazyEventListener();

    String sourceURL() const final { return m_sourceURL; }
    TextPosition sourcePosition() const final { return m_sourcePosition; }

private:
    struct CreationArguments;
    static RefPtr<JSLazyEventListener> create(CreationArguments&&);
    JSLazyEventListener(CreationArguments&&, const String& sourceURL, const TextPosition&);

#if !ASSERT_DISABLED
    void checkValidityForEventTarget(EventTarget&) final;
#endif

    JSC::JSObject* initializeJSFunction(ScriptExecutionContext&) const final;
    bool wasCreatedFromMarkup() const final { return true; }

    String m_functionName;
    const String& m_eventParameterName;
    String m_code;
    String m_sourceURL;
    TextPosition m_sourcePosition;
    WeakPtr<ContainerNode> m_originalNode;
};

} // namespace WebCore
