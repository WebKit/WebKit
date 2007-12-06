// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "JSClassRef.h"

#include "APICast.h"
#include "JSCallbackObject.h"
#include "JSObjectRef.h"
#include <kjs/JSGlobalObject.h>
#include <kjs/identifier.h>
#include <kjs/object_object.h>

using namespace KJS;

const JSClassDefinition kJSClassDefinitionEmpty = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

OpaqueJSClass::OpaqueJSClass(const JSClassDefinition* definition, OpaqueJSClass* protoClass) 
    // FIXME: <rdar://problem/4949018>
    : className(definition->className)
    , parentClass(definition->parentClass)
    , prototypeClass(0)
    , staticValues(0)
    , staticFunctions(0)
    , initialize(definition->initialize)
    , finalize(definition->finalize)
    , hasProperty(definition->hasProperty)
    , getProperty(definition->getProperty)
    , setProperty(definition->setProperty)
    , deleteProperty(definition->deleteProperty)
    , getPropertyNames(definition->getPropertyNames)
    , callAsFunction(definition->callAsFunction)
    , callAsConstructor(definition->callAsConstructor)
    , hasInstance(definition->hasInstance)
    , convertToType(definition->convertToType)
    , cachedPrototype(0)
{
    if (const JSStaticValue* staticValue = definition->staticValues) {
        staticValues = new StaticValuesTable();
        while (staticValue->name) {
            // FIXME: <rdar://problem/4949018>
            staticValues->add(Identifier(staticValue->name).ustring().rep(), 
                              new StaticValueEntry(staticValue->getProperty, staticValue->setProperty, staticValue->attributes));
            ++staticValue;
        }
    }
    
    if (const JSStaticFunction* staticFunction = definition->staticFunctions) {
        staticFunctions = new StaticFunctionsTable();
        while (staticFunction->name) {
            // FIXME: <rdar://problem/4949018>
            staticFunctions->add(Identifier(staticFunction->name).ustring().rep(), 
                                 new StaticFunctionEntry(staticFunction->callAsFunction, staticFunction->attributes));
            ++staticFunction;
        }
    }
        
    if (protoClass)
        prototypeClass = JSClassRetain(protoClass);
}

OpaqueJSClass::~OpaqueJSClass()
{
    if (staticValues) {
        deleteAllValues(*staticValues);
        delete staticValues;
    }

    if (staticFunctions) {
        deleteAllValues(*staticFunctions);
        delete staticFunctions;
    }
    
    if (prototypeClass)
        JSClassRelease(prototypeClass);
}

JSClassRef OpaqueJSClass::createNoAutomaticPrototype(const JSClassDefinition* definition)
{
    return new OpaqueJSClass(definition, 0);
}

void clearReferenceToPrototype(JSObjectRef prototype)
{
    OpaqueJSClass* jsClass = static_cast<OpaqueJSClass*>(JSObjectGetPrivate(prototype));
    ASSERT(jsClass);
    jsClass->cachedPrototype = 0;
}

JSClassRef OpaqueJSClass::create(const JSClassDefinition* definition)
{
    if (const JSStaticFunction* staticFunctions = definition->staticFunctions) {
        // copy functions into a prototype class
        JSClassDefinition protoDefinition = kJSClassDefinitionEmpty;
        protoDefinition.staticFunctions = staticFunctions;
        protoDefinition.finalize = clearReferenceToPrototype;
        OpaqueJSClass* protoClass = new OpaqueJSClass(&protoDefinition, 0);

        // remove functions from the original class
        JSClassDefinition objectDefinition = *definition;
        objectDefinition.staticFunctions = 0;
        return new OpaqueJSClass(&objectDefinition, protoClass);
    }

    return new OpaqueJSClass(definition, 0);
}

/*!
// Doc here in case we make this public. (Hopefully we won't.)
@function
 @abstract Returns the prototype that will be used when constructing an object with a given class.
 @param ctx The execution context to use.
 @param jsClass A JSClass whose prototype you want to get.
 @result The JSObject prototype that was automatically generated for jsClass, or NULL if no prototype was automatically generated. This is the prototype that will be used when constructing an object using jsClass.
*/
JSObject* OpaqueJSClass::prototype(JSContextRef ctx)
{
    /* Class (C++) and prototype (JS) inheritance are parallel, so:
     *     (C++)      |        (JS)
     *   ParentClass  |   ParentClassPrototype
     *       ^        |          ^
     *       |        |          |
     *  DerivedClass  |  DerivedClassPrototype
     */
    
    if (!prototypeClass)
        return 0;
    
    ExecState* exec = toJS(ctx);
    
    if (!cachedPrototype) {
        // Recursive, but should be good enough for our purposes
        JSObject* parentPrototype = 0;
        if (parentClass)
            parentPrototype = parentClass->prototype(ctx); // can be null
        if (!parentPrototype)
            parentPrototype = exec->dynamicGlobalObject()->objectPrototype();
        cachedPrototype = new JSCallbackObject<JSObject>(exec, prototypeClass, parentPrototype, this); // set ourself as the object's private data, so it can clear our reference on destruction
    }
    return cachedPrototype;
}
