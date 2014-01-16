/*
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2008, 2009, 2013 Apple Inc. All rights reserved.
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

#ifndef JSLazyEventListener_h
#define JSLazyEventListener_h

#include "JSEventListener.h"
#include <wtf/text/TextPosition.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

    class ContainerNode;
    class Frame;
    class QualifiedName;

    class JSLazyEventListener FINAL : public JSEventListener {
    public:
        static PassRefPtr<JSLazyEventListener> createForNode(ContainerNode&, const QualifiedName& attributeName, const AtomicString& attributeValue);
        static PassRefPtr<JSLazyEventListener> createForDOMWindow(Frame&, const QualifiedName& attributeName, const AtomicString& attributeValue);

        virtual ~JSLazyEventListener();

    private:
        JSLazyEventListener(const String& functionName, const String& eventParameterName, const String& code, ContainerNode*, const String& sourceURL, const TextPosition&, JSC::JSObject* wrapper, DOMWrapperWorld& isolatedWorld);

        virtual JSC::JSObject* initializeJSFunction(ScriptExecutionContext*) const override;
        virtual bool wasCreatedFromMarkup() const override { return true; }

        static void create() WTF_DELETED_FUNCTION;

        mutable String m_functionName;
        mutable String m_eventParameterName;
        mutable String m_code;
        mutable String m_sourceURL;
        TextPosition m_position;
        ContainerNode* m_originalNode;
    };

} // namespace WebCore

#endif // JSLazyEventListener_h
