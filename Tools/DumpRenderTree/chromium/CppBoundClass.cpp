/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2009 Pawel Hajdan (phajdan.jr@chromium.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// This file contains definitions for CppBoundClass

// Here's the control flow of a JS method getting forwarded to a class.
// - Something calls our NPObject with a function like "Invoke".
// - CppNPObject's static invoke() function forwards it to its attached
//   CppBoundClass's invoke() method.
// - CppBoundClass has then overridden invoke() to look up the function
//   name in its internal map of methods, and then calls the appropriate
//   method.

#include "config.h"
#include "CppBoundClass.h"

#include "WebBindings.h"
#include "WebFrame.h"
#include "WebString.h"
#include <wtf/Assertions.h>
#include <wtf/OwnPtr.h>

using namespace WebKit;
using namespace std;

class CppVariantPropertyCallback : public CppBoundClass::PropertyCallback {
public:
    CppVariantPropertyCallback(CppVariant* value) : m_value(value) { }

    virtual bool getValue(CppVariant* value)
    {
        value->set(*m_value);
        return true;
    }

    virtual bool setValue(const CppVariant& value)
    {
        m_value->set(value);
        return true;
    }

private:
    CppVariant* m_value;
};

class GetterPropertyCallback : public CppBoundClass::PropertyCallback {
public:
    GetterPropertyCallback(CppBoundClass::GetterCallback* callback)
        : m_callback(callback) { }

    virtual bool getValue(CppVariant* value)
    {
        m_callback->run(value);
        return true;
    }

    virtual bool setValue(const CppVariant& value) { return false; }

private:
    OwnPtr<CppBoundClass::GetterCallback> m_callback;
};

// Our special NPObject type.  We extend an NPObject with a pointer to a
// CppBoundClass, which is just a C++ interface that we forward all NPObject
// callbacks to.
struct CppNPObject {
    NPObject parent; // This must be the first field in the struct.
    CppBoundClass* boundClass;

    //
    // All following objects and functions are static, and just used to interface
    // with NPObject/NPClass.
    //

    // An NPClass associates static functions of CppNPObject with the
    // function pointers used by the JS runtime.
    static NPClass npClass;

    // Allocate a new NPObject with the specified class.
    static NPObject* allocate(NPP, NPClass*);

    // Free an object.
    static void deallocate(NPObject*);

    // Returns true if the C++ class associated with this NPObject exposes the
    // given property.  Called by the JS runtime.
    static bool hasProperty(NPObject*, NPIdentifier);

    // Returns true if the C++ class associated with this NPObject exposes the
    // given method.  Called by the JS runtime.
    static bool hasMethod(NPObject*, NPIdentifier);

    // If the given method is exposed by the C++ class associated with this
    // NPObject, invokes it with the given arguments and returns a result.  Otherwise,
    // returns "undefined" (in the JavaScript sense).  Called by the JS runtime.
    static bool invoke(NPObject*, NPIdentifier,
                       const NPVariant* arguments, uint32_t argumentCount,
                       NPVariant* result);

    // If the given property is exposed by the C++ class associated with this
    // NPObject, returns its value.  Otherwise, returns "undefined" (in the
    // JavaScript sense).  Called by the JS runtime.
    static bool getProperty(NPObject*, NPIdentifier, NPVariant* result);

    // If the given property is exposed by the C++ class associated with this
    // NPObject, sets its value.  Otherwise, does nothing. Called by the JS
    // runtime.
    static bool setProperty(NPObject*, NPIdentifier, const NPVariant* value);
};

// Build CppNPObject's static function pointers into an NPClass, for use
// in constructing NPObjects for the C++ classes.
NPClass CppNPObject::npClass = {
    NP_CLASS_STRUCT_VERSION,
    CppNPObject::allocate,
    CppNPObject::deallocate,
    /* NPInvalidateFunctionPtr */ 0,
    CppNPObject::hasMethod,
    CppNPObject::invoke,
    /* NPInvokeDefaultFunctionPtr */ 0,
    CppNPObject::hasProperty,
    CppNPObject::getProperty,
    CppNPObject::setProperty,
    /* NPRemovePropertyFunctionPtr */ 0
};

NPObject* CppNPObject::allocate(NPP npp, NPClass* aClass)
{
    CppNPObject* obj = new CppNPObject;
    // obj->parent will be initialized by the NPObject code calling this.
    obj->boundClass = 0;
    return &obj->parent;
}

void CppNPObject::deallocate(NPObject* npObj)
{
    CppNPObject* obj = reinterpret_cast<CppNPObject*>(npObj);
    delete obj;
}

bool CppNPObject::hasMethod(NPObject* npObj, NPIdentifier ident)
{
    CppNPObject* obj = reinterpret_cast<CppNPObject*>(npObj);
    return obj->boundClass->hasMethod(ident);
}

bool CppNPObject::hasProperty(NPObject* npObj, NPIdentifier ident)
{
    CppNPObject* obj = reinterpret_cast<CppNPObject*>(npObj);
    return obj->boundClass->hasProperty(ident);
}

bool CppNPObject::invoke(NPObject* npObj, NPIdentifier ident,
                         const NPVariant* arguments, uint32_t argumentCount,
                         NPVariant* result)
{
    CppNPObject* obj = reinterpret_cast<CppNPObject*>(npObj);
    return obj->boundClass->invoke(ident, arguments, argumentCount, result);
}

bool CppNPObject::getProperty(NPObject* npObj, NPIdentifier ident, NPVariant* result)
{
    CppNPObject* obj = reinterpret_cast<CppNPObject*>(npObj);
    return obj->boundClass->getProperty(ident, result);
}

bool CppNPObject::setProperty(NPObject* npObj, NPIdentifier ident, const NPVariant* value)
{
    CppNPObject* obj = reinterpret_cast<CppNPObject*>(npObj);
    return obj->boundClass->setProperty(ident, value);
}

CppBoundClass::~CppBoundClass()
{
    for (MethodList::iterator i = m_methods.begin(); i != m_methods.end(); ++i)
        delete i->second;

    for (PropertyList::iterator i = m_properties.begin(); i != m_properties.end(); ++i)
        delete i->second;

    // Unregister ourselves if we were bound to a frame.
    if (m_boundToFrame)
        WebBindings::unregisterObject(NPVARIANT_TO_OBJECT(m_selfVariant));
}

bool CppBoundClass::hasMethod(NPIdentifier ident) const
{
    return m_methods.find(ident) != m_methods.end();
}

bool CppBoundClass::hasProperty(NPIdentifier ident) const
{
    return m_properties.find(ident) != m_properties.end();
}

bool CppBoundClass::invoke(NPIdentifier ident,
                           const NPVariant* arguments,
                           size_t argumentCount,
                           NPVariant* result) {
    MethodList::const_iterator end = m_methods.end();
    MethodList::const_iterator method = m_methods.find(ident);
    Callback* callback;
    if (method == end) {
        if (!m_fallbackCallback.get()) {
            VOID_TO_NPVARIANT(*result);
            return false;
        }
        callback = m_fallbackCallback.get();
    } else
        callback = (*method).second;

    // Build a CppArgumentList argument vector from the NPVariants coming in.
    CppArgumentList cppArguments(argumentCount);
    for (size_t i = 0; i < argumentCount; i++)
        cppArguments[i].set(arguments[i]);

    CppVariant cppResult;
    callback->run(cppArguments, &cppResult);

    cppResult.copyToNPVariant(result);
    return true;
}

bool CppBoundClass::getProperty(NPIdentifier ident, NPVariant* result) const
{
    PropertyList::const_iterator callback = m_properties.find(ident);
    if (callback == m_properties.end()) {
        VOID_TO_NPVARIANT(*result);
        return false;
    }

    CppVariant cppValue;
    if (!callback->second->getValue(&cppValue))
        return false;
    cppValue.copyToNPVariant(result);
    return true;
}

bool CppBoundClass::setProperty(NPIdentifier ident, const NPVariant* value)
{
    PropertyList::iterator callback = m_properties.find(ident);
    if (callback == m_properties.end())
        return false;

    CppVariant cppValue;
    cppValue.set(*value);
    return (*callback).second->setValue(cppValue);
}

void CppBoundClass::bindCallback(const string& name, Callback* callback)
{
    NPIdentifier ident = WebBindings::getStringIdentifier(name.c_str());
    MethodList::iterator oldCallback = m_methods.find(ident);
    if (oldCallback != m_methods.end()) {
        delete oldCallback->second;
        if (!callback) {
            m_methods.remove(oldCallback);
            return;
        }
    }

    m_methods.set(ident, callback);
}

void CppBoundClass::bindGetterCallback(const string& name, GetterCallback* callback)
{
    PropertyCallback* propertyCallback = callback ? new GetterPropertyCallback(callback) : 0;
    bindProperty(name, propertyCallback);
}

void CppBoundClass::bindProperty(const string& name, CppVariant* prop)
{
    PropertyCallback* propertyCallback = prop ? new CppVariantPropertyCallback(prop) : 0;
    bindProperty(name, propertyCallback);
}

void CppBoundClass::bindProperty(const string& name, PropertyCallback* callback)
{
    NPIdentifier ident = WebBindings::getStringIdentifier(name.c_str());
    PropertyList::iterator oldCallback = m_properties.find(ident);
    if (oldCallback != m_properties.end()) {
        delete oldCallback->second;
        if (!callback) {
            m_properties.remove(oldCallback);
            return;
        }
    }

    m_properties.set(ident, callback);
}

bool CppBoundClass::isMethodRegistered(const string& name) const
{
    NPIdentifier ident = WebBindings::getStringIdentifier(name.c_str());
    MethodList::const_iterator callback = m_methods.find(ident);
    return callback != m_methods.end();
}

CppVariant* CppBoundClass::getAsCppVariant()
{
    if (!m_selfVariant.isObject()) {
        // Create an NPObject using our static NPClass.  The first argument (a
        // plugin's instance handle) is passed through to the allocate function
        // directly, and we don't use it, so it's ok to be 0.
        NPObject* npObj = WebBindings::createObject(0, &CppNPObject::npClass);
        CppNPObject* obj = reinterpret_cast<CppNPObject*>(npObj);
        obj->boundClass = this;
        m_selfVariant.set(npObj);
        WebBindings::releaseObject(npObj); // CppVariant takes the reference.
    }
    ASSERT(m_selfVariant.isObject());
    return &m_selfVariant;
}

void CppBoundClass::bindToJavascript(WebFrame* frame, const WebString& classname)
{
    // BindToWindowObject will take its own reference to the NPObject, and clean
    // up after itself.  It will also (indirectly) register the object with V8,
    // so we must remember this so we can unregister it when we're destroyed.
    frame->bindToWindowObject(classname, NPVARIANT_TO_OBJECT(*getAsCppVariant()));
    m_boundToFrame = true;
}
