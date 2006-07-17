// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "JSNode.h"
#include "JSNodeList.h"
#include "Node.h"
#include "NodeList.h"
#include "UnusedParam.h"

static JSClassRef JSNode_class(JSContextRef context);

static JSValueRef JSNodePrototype_appendChild(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(function);

    // Example of throwing a type error for invalid values
    if (!JSValueIsObjectOfClass(context, thisObject, JSNode_class(context))) {
        JSStringRef message = JSStringCreateWithUTF8CString("TypeError: appendChild can only be called on nodes");
        *exception = JSValueMakeString(context, message);
        JSStringRelease(message);
    } else if (argumentCount < 1 || !JSValueIsObjectOfClass(context, arguments[0], JSNode_class(context))) {
        JSStringRef message = JSStringCreateWithUTF8CString("TypeError: first argument to appendChild must be a node");
        *exception = JSValueMakeString(context, message);
        JSStringRelease(message);
    } else {
        Node* node = JSObjectGetPrivate(thisObject);
        Node* child = JSObjectGetPrivate(JSValueToObject(context, arguments[0], NULL));

        Node_appendChild(node, child);
    }

    return JSValueMakeUndefined(context);
}

static JSValueRef JSNodePrototype_removeChild(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(function);
    
    // Example of ignoring invalid values
    if (argumentCount > 0) {
        if (JSValueIsObjectOfClass(context, thisObject, JSNode_class(context))) {
            if (JSValueIsObjectOfClass(context, arguments[0], JSNode_class(context))) {
                Node* node = JSObjectGetPrivate(thisObject);
                Node* child = JSObjectGetPrivate(JSValueToObject(context, arguments[0], NULL));
                
                Node_removeChild(node, child);
            }
        }
    }
    
    return JSValueMakeUndefined(context);
}

static JSValueRef JSNodePrototype_replaceChild(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(function);
    
    if (argumentCount > 1) {
        if (JSValueIsObjectOfClass(context, thisObject, JSNode_class(context))) {
            if (JSValueIsObjectOfClass(context, arguments[0], JSNode_class(context))) {
                if (JSValueIsObjectOfClass(context, arguments[1], JSNode_class(context))) {
                    Node* node = JSObjectGetPrivate(thisObject);
                    Node* newChild = JSObjectGetPrivate(JSValueToObject(context, arguments[0], NULL));
                    Node* oldChild = JSObjectGetPrivate(JSValueToObject(context, arguments[1], NULL));
                    
                    Node_replaceChild(node, newChild, oldChild);
                }
            }
        }
    }
    
    return JSValueMakeUndefined(context);
}

static JSStaticFunction JSNodePrototype_staticFunctions[] = {
    { "appendChild", JSNodePrototype_appendChild, kJSPropertyAttributeDontDelete },
    { "removeChild", JSNodePrototype_removeChild, kJSPropertyAttributeDontDelete },
    { "replaceChild", JSNodePrototype_replaceChild, kJSPropertyAttributeDontDelete },
    { 0, 0, 0 }
};

static JSClassRef JSNodePrototype_class(JSContextRef context)
{
    static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionNull;
        definition.staticFunctions = JSNodePrototype_staticFunctions;
        jsClass = JSClassCreate(&definition);
    }
    return jsClass;
}

static JSValueRef JSNode_getNodeType(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(propertyName);

    Node* node = JSObjectGetPrivate(object);
    if (node) {
        JSStringRef nodeType = JSStringCreateWithUTF8CString(node->nodeType);
        JSValueRef value = JSValueMakeString(context, nodeType);
        JSStringRelease(nodeType);
        return value;
    }
    
    return NULL;
}

static JSValueRef JSNode_getChildNodes(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(propertyName);
    Node* node = JSObjectGetPrivate(thisObject);
    assert(node);
    return JSNodeList_new(context, NodeList_new(node));
}

static JSValueRef JSNode_getFirstChild(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(propertyName);
    UNUSED_PARAM(object);
    
    return JSValueMakeUndefined(context);
}

static JSStaticValue JSNode_staticValues[] = {
    { "nodeType", JSNode_getNodeType, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
    { "childNodes", JSNode_getChildNodes, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
    { "firstChild", JSNode_getFirstChild, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
    { 0, 0, 0, 0 }
};

static void JSNode_finalize(JSObjectRef object)
{
    Node* node = JSObjectGetPrivate(object);
    assert(node);

    Node_deref(node);
}

static JSClassRef JSNode_class(JSContextRef context)
{
    static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionNull;
        definition.staticValues = JSNode_staticValues;
        definition.finalize = JSNode_finalize;

        jsClass = JSClassCreate(&definition);
    }
    return jsClass;
}

JSObjectRef JSNode_prototype(JSContextRef context)
{
    static JSObjectRef prototype;
    if (!prototype) {
        prototype = JSObjectMake(context, JSNodePrototype_class(context), NULL, NULL);
        JSValueProtect(context, prototype);
    }
    return prototype;
}

JSObjectRef JSNode_new(JSContextRef context, Node* node)
{
    Node_ref(node);

    JSObjectRef jsNode = JSObjectMake(context, JSNode_class(context), JSNode_prototype(context), NULL);
    JSObjectSetPrivate(jsNode, node);
    return jsNode;
}

JSObjectRef JSNode_construct(JSContextRef context, JSObjectRef object, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    UNUSED_PARAM(object);
    UNUSED_PARAM(argumentCount);
    UNUSED_PARAM(arguments);

    return JSNode_new(context, Node_new());
}
