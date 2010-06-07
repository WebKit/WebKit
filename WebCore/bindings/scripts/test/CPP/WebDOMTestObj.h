/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef WebDOMTestObj_h
#define WebDOMTestObj_h

#include <WebDOMObject.h>
#include <WebDOMString.h>

namespace WebCore {
class TestObj;
};

class WebDOMEventListener;
class WebDOMString;
class WebDOMTestObj;

class WebDOMTestObj : public WebDOMObject {
public:
    WebDOMTestObj();
    explicit WebDOMTestObj(WebCore::TestObj*);
    WebDOMTestObj(const WebDOMTestObj&);
    ~WebDOMTestObj();

    int readOnlyIntAttr() const;
    WebDOMString readOnlyStringAttr() const;
    WebDOMTestObj readOnlyTestObjAttr() const;
    int intAttr() const;
    void setIntAttr(int);
    long long longLongAttr() const;
    void setLongLongAttr(long long);
    unsigned long long unsignedLongLongAttr() const;
    void setUnsignedLongLongAttr(unsigned long long);
    WebDOMString stringAttr() const;
    void setStringAttr(const WebDOMString&);
    WebDOMTestObj testObjAttr() const;
    void setTestObjAttr(const WebDOMTestObj&);
    int attrWithException() const;
    void setAttrWithException(int);
    int attrWithSetterException() const;
    void setAttrWithSetterException(int);
    int attrWithGetterException() const;
    void setAttrWithGetterException(int);
    int customAttr() const;
    void setCustomAttr(int);
    WebDOMString scriptStringAttr() const;

    void voidMethod();
    void voidMethodWithArgs(int intArg, const WebDOMString& strArg, const WebDOMTestObj& objArg);
    int intMethod();
    int intMethodWithArgs(int intArg, const WebDOMString& strArg, const WebDOMTestObj& objArg);
    WebDOMTestObj objMethod();
    WebDOMTestObj objMethodWithArgs(int intArg, const WebDOMString& strArg, const WebDOMTestObj& objArg);
    WebDOMTestObj methodThatRequiresAllArgs(const WebDOMString& strArg, const WebDOMTestObj& objArg);
    WebDOMTestObj methodThatRequiresAllArgsAndThrows(const WebDOMString& strArg, const WebDOMTestObj& objArg);
    void serializedValue(const WebDOMString& serializedArg);
    void methodWithException();
    void customMethod();
    void customMethodWithArgs(int intArg, const WebDOMString& strArg, const WebDOMTestObj& objArg);
    void addEventListener(const WebDOMString& type, const WebDOMEventListener& listener, bool useCapture);
    void removeEventListener(const WebDOMString& type, const WebDOMEventListener& listener, bool useCapture);
    void withDynamicFrame();
    void withDynamicFrameAndArg(int intArg);
    void withDynamicFrameAndOptionalArg(int intArg, int optionalArg);
    void withDynamicFrameAndUserGesture(int intArg);
    void withDynamicFrameAndUserGestureASAD(int intArg, int optionalArg);
    void withScriptStateVoid();
    WebDOMTestObj withScriptStateObj();
    void withScriptStateVoidException();
    WebDOMTestObj withScriptStateObjException();
    void methodWithOptionalArg(int opt);
    void methodWithNonOptionalArgAndOptionalArg(int nonOpt, int opt);
    void methodWithNonOptionalArgAndTwoOptionalArgs(int nonOpt, int opt1, int opt2);

    WebCore::TestObj* impl() const;

protected:
    struct WebDOMTestObjPrivate;
    WebDOMTestObjPrivate* m_impl;
};

WebCore::TestObj* toWebCore(const WebDOMTestObj&);
WebDOMTestObj toWebKit(WebCore::TestObj*);

#endif
