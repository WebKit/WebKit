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

static JSValueRef JSNodePrototype_appendChild(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argc, JSValueRef argv[], JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(function);

    // Example of throwing a type error for invalid values
    if (!JSValueIsObjectOfClass(thisObject, JSNode_class(context))) {
        JSInternalStringRef message = JSInternalStringCreateUTF8("TypeError: appendChild can only be called on nodes");
        *exception = JSStringMake(message);
        JSInternalStringRelease(message);
    } else if (argc < 1 || !JSValueIsObjectOfClass(argv[0], JSNode_class(context))) {
        JSInternalStringRef message = JSInternalStringCreateUTF8("TypeError: first argument to appendChild must be a node");
        *exception = JSStringMake(message);
        JSInternalStringRelease(message);
    } else {
        Node* node = JSObjectGetPrivate(thisObject);
        Node* child = JSObjectGetPrivate(JSValueToObject(context, argv[0]));

        Node_appendChild(node, child);
    }

    return JSUndefinedMake();
}

static JSValueRef JSNodePrototype_removeChild(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argc, JSValueRef argv[], JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(function);
    
    // Example of ignoring invalid values
    if (argc > 0) {
        if (JSValueIsObjectOfClass(thisObject, JSNode_class(context))) {
            if (JSValueIsObjectOfClass(argv[0], JSNode_class(context))) {
                Node* node = JSObjectGetPrivate(thisObject);
                Node* child = JSObjectGetPrivate(JSValueToObject(context, argv[0]));
                
                Node_removeChild(node, child);
            }
        }
    }
    
    return JSUndefinedMake();
}

static JSValueRef JSNodePrototype_replaceChild(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argc, JSValueRef argv[], JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(function);
    
    if (argc > 1) {
        if (JSValueIsObjectOfClass(thisObject, JSNode_class(context))) {
            if (JSValueIsObjectOfClass(argv[0], JSNode_class(context))) {
                if (JSValueIsObjectOfClass(argv[1], JSNode_class(context))) {
                    Node* node = JSObjectGetPrivate(thisObject);
                    Node* newChild = JSObjectGetPrivate(JSValueToObject(context, argv[0]));
                    Node* oldChild = JSObjectGetPrivate(JSValueToObject(context, argv[1]));
                    
                    Node_replaceChild(node, newChild, oldChild);
                }
            }
        }
    }
    
    return JSUndefinedMake();
}

static JSStaticFunction JSNodePrototype_staticFunctions[] = {
    { "appendChild", JSNodePrototype_appendChild, kJSPropertyAttributeDontDelete },
    { "removeChild", JSNodePrototype_removeChild, kJSPropertyAttributeDontDelete },
    { "replaceChild", JSNodePrototype_replaceChild, kJSPropertyAttributeDontDelete },
    { 0, 0, 0 }
};

static JSClassRef JSNodePrototype_class(JSContextRef context)
{
    static JSClassRef nodePrototypeClass;
    if (!nodePrototypeClass)
        nodePrototypeClass = JSClassCreate(NULL, JSNodePrototype_staticFunctions, &kJSObjectCallbacksNone, NULL);
    return nodePrototypeClass;
}

static bool JSNode_getNodeType(JSContextRef context, JSObjectRef object, JSInternalStringRef propertyName, JSValueRef* returnValue, JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(propertyName);

    Node* node = JSObjectGetPrivate(object);
    if (node) {
        JSInternalStringRef nodeType = JSInternalStringCreateUTF8(node->nodeType);
        *returnValue = JSStringMake(nodeType);
        JSInternalStringRelease(nodeType);
        return true;
    }
    return false;
}

static bool JSNode_getChildNodes(JSContextRef context, JSObjectRef thisObject, JSInternalStringRef propertyName, JSValueRef* returnValue, JSValueRef* exception)
{
    UNUSED_PARAM(propertyName);
    Node* node = JSObjectGetPrivate(thisObject);
    assert(node);
    *returnValue = JSNodeList_new(context, NodeList_new(node));
    return true;
}

static bool JSNode_getFirstChild(JSContextRef context, JSObjectRef object, JSInternalStringRef propertyName, JSValueRef* returnValue, JSValueRef* exception)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(propertyName);
    UNUSED_PARAM(object);
    
    *returnValue = JSUndefinedMake();
    return true;
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
    static JSClassRef nodeClass;
    if (!nodeClass) {
        JSObjectCallbacks JSNode_callbacks = kJSObjectCallbacksNone;
        JSNode_callbacks.finalize = JSNode_finalize;
        
        nodeClass = JSClassCreate(JSNode_staticValues, NULL, &JSNode_callbacks, NULL);
    }
    return nodeClass;
}

static JSObjectRef JSNode_prototype(JSContextRef context)
{
    static JSObjectRef prototype;
    if (!prototype) {
        prototype = JSObjectMake(context, JSNodePrototype_class(context), NULL);
        JSGCProtect(prototype);
    }
    return prototype;
}

JSObjectRef JSNode_new(JSContextRef context, Node* node)
{
    Node_ref(node);

    JSObjectRef jsNode = JSObjectMake(context, JSNode_class(context), JSNode_prototype(context));
    JSObjectSetPrivate(jsNode, node);
    return jsNode;
}

JSObjectRef JSNode_construct(JSContextRef context, JSObjectRef object, size_t argc, JSValueRef argv[], JSValueRef* exception)
{
    UNUSED_PARAM(object);
    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);

    return JSNode_new(context, Node_new());
}
