/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
#include <CoreFoundation/CoreFoundation.h>

#include <identifier.h>
#include <internal.h>
#include <interpreter.h>
#include <list.h>
#include <jni_jsobject.h>
#include <jni_runtime.h>
#include <jni_utility.h>

using namespace Bindings;
using namespace KJS;

static KJSFindRootObjectForNativeHandleFunctionPtr findRootObjectForNativeHandleFunctionPtr = 0;

void KJS_setFindRootObjectForNativeHandleFunction(KJSFindRootObjectForNativeHandleFunctionPtr aFunc)
{
    findRootObjectForNativeHandleFunctionPtr = aFunc;
}

KJSFindRootObjectForNativeHandleFunctionPtr KJS_findRootObjectForNativeHandleFunction()
{
    return findRootObjectForNativeHandleFunctionPtr;
}

// Java does NOT always call finalize (and thus KJS_JSObject_JSFinalize) when
// it collects an objects.  This presents some difficulties.  We must ensure
// the a JSObject's corresponding JavaScript object doesn't get collected.  We
// do this by incrementing the JavaScript's reference count the first time we
// create a JSObject for it, and decrementing the JavaScript reference count when
// the last JSObject that refers to it is finalized, or when the applet is
// shutdown.
//
// To do this we keep a dictionary that maps each applet instance
// to the JavaScript objects it is referencing.  For each JavaScript instance
// we also maintain a secondary reference count.  When that reference count reaches
// 1 OR the applet is shutdown we deref the JavaScript instance.  Applet instances
// are represented by a jlong.

static CFMutableDictionaryRef referencesByRootDictionary = 0;

static CFMutableDictionaryRef getReferencesByRootDictionary()
{
    if (!referencesByRootDictionary)
        referencesByRootDictionary = CFDictionaryCreateMutable(NULL, 0, NULL, &kCFTypeDictionaryValueCallBacks);
    return referencesByRootDictionary;
}

static CFMutableDictionaryRef getReferencesDictionary(const Bindings::RootObject *root)
{
    CFMutableDictionaryRef refsByRoot = getReferencesByRootDictionary();
    CFMutableDictionaryRef referencesDictionary = 0;
    
    referencesDictionary = (CFMutableDictionaryRef)CFDictionaryGetValue (refsByRoot, (const void *)root);
    if (!referencesDictionary) {
        referencesDictionary = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
        CFDictionaryAddValue (refsByRoot, root, referencesDictionary);
        CFRelease (referencesDictionary);
    }
    return referencesDictionary;
}

// Scan all the dictionary for all the roots to see if any have a 
// reference to the imp, and if so, return it's reference count
// dictionary.
// FIXME:  This is a potential performance bottleneck with many applets.  We could fix be adding a
// imp to root dictionary.
static CFMutableDictionaryRef findReferenceDictionary(ObjectImp *imp)
{
    CFMutableDictionaryRef refsByRoot = getReferencesByRootDictionary ();
    CFMutableDictionaryRef foundDictionary = 0;
    
    if (refsByRoot) {
        const void **allValues = 0;
        CFIndex count, i;
        
        count = CFDictionaryGetCount(refsByRoot);
        allValues = (const void **)malloc (sizeof(void *) * count);
        CFDictionaryGetKeysAndValues (refsByRoot, NULL, allValues);
        for(i = 0; i < count; i++) {
            CFMutableDictionaryRef referencesDictionary = (CFMutableDictionaryRef)allValues[i];
            if (CFDictionaryGetValue(referencesDictionary, imp) != 0) {
                foundDictionary = referencesDictionary;
                break;
            }
        }
        
        free ((void *)allValues);
    }
    return foundDictionary;
}

// FIXME:  This is a potential performance bottleneck with many applets.  We could fix be adding a
// imp to root dictionary.
const Bindings::RootObject *rootForImp (ObjectImp *imp)
{
    CFMutableDictionaryRef refsByRoot = getReferencesByRootDictionary ();
    const Bindings::RootObject *rootObject = 0;
    
    if (refsByRoot) {
        const void **allValues = 0;
        const void **allKeys = 0;
        CFIndex count, i;
        
        count = CFDictionaryGetCount(refsByRoot);
        allKeys = (const void **)malloc (sizeof(void *) * count);
        allValues = (const void **)malloc (sizeof(void *) * count);
        CFDictionaryGetKeysAndValues (refsByRoot, allKeys, allValues);
        for(i = 0; i < count; i++) {
            CFMutableDictionaryRef referencesDictionary = (CFMutableDictionaryRef)allValues[i];
            if (CFDictionaryGetValue(referencesDictionary, imp) != 0) {
                rootObject = (const Bindings::RootObject *)allKeys[0];
                break;
            }
        }
        
        free ((void *)allKeys);
        free ((void *)allValues);
    }
    return rootObject;
}

static void addJavaReference (const Bindings::RootObject *root, ObjectImp *imp)
{
    CFMutableDictionaryRef referencesDictionary = getReferencesDictionary (root);
    
    unsigned int numReferences = (unsigned int)CFDictionaryGetValue (referencesDictionary, imp);
    if (numReferences == 0) {
        imp->ref();
        CFDictionaryAddValue (referencesDictionary, imp,  (const void *)1);
    }
    else {
        CFDictionaryReplaceValue (referencesDictionary, imp, (const void *)(numReferences+1));
    }
}

static void removeJavaReference (ObjectImp *imp)
{
    CFMutableDictionaryRef referencesDictionary = findReferenceDictionary (imp);
    
    unsigned int numReferences = (unsigned int)CFDictionaryGetValue (referencesDictionary, imp);
    if (numReferences == 1) {
        imp->deref();
        CFDictionaryRemoveValue (referencesDictionary, imp);
    }
    else {
        CFDictionaryReplaceValue (referencesDictionary, imp, (const void *)(numReferences-1));
    }
}

extern "C" {

// Must be called when the applet is shutdown.
void KJS_removeAllJavaReferencesForRoot (Bindings::RootObject *root)
{
    CFMutableDictionaryRef referencesDictionary = getReferencesDictionary (root);
    
    if (referencesDictionary) {
        void **allImps = 0;
        CFIndex count, i;
        
        count = CFDictionaryGetCount(referencesDictionary);
        CFDictionaryGetKeysAndValues (referencesDictionary, (const void **)allImps, NULL);
        allImps = (void **)malloc (sizeof(void *) * count);
        for(i = 0; i < count; i++) {
            ObjectImp *anImp = static_cast<ObjectImp*>(allImps[i]);
            anImp->deref();
        }
        free ((void *)allImps);
        CFDictionaryRemoveAllValues (referencesDictionary);

        CFMutableDictionaryRef refsByRoot = getReferencesByRootDictionary();
        CFDictionaryRemoveValue (refsByRoot, (const void *)root);
        delete root;
    }
}

jlong KJS_JSCreateNativeJSObject (JNIEnv *env, jclass clazz, jstring jurl, jlong nativeHandle, jboolean ctx)
{
    KJSFindRootObjectForNativeHandleFunctionPtr aFunc = KJS_findRootObjectForNativeHandleFunction();
    if (aFunc) {
        Bindings::RootObject *root = aFunc(jlong_to_ptr(nativeHandle));
        addJavaReference (root, root->rootObjectImp());        
        return ptr_to_jlong(root->rootObjectImp());
    }
    
    return ptr_to_jlong(0);
}

void KJS_JSObject_JSFinalize (JNIEnv *env, jclass jsClass, jlong nativeJSObject)
{
    removeJavaReference (jlong_to_impptr(nativeJSObject));
}

jobject KJS_JSObject_JSObjectCall (JNIEnv *env, jclass jsClass, jlong nativeJSObject, jstring jurl, jstring mName, jobjectArray args, jboolean ctx)
{
    ObjectImp *imp = jlong_to_impptr(nativeJSObject);
    const Bindings::RootObject *root = rootForImp (imp);
    
    // Change to assert.
    if (!root) {
        return 0;
    }
    
    // Lookup the function object.
    ExecState *exec = root->interpreter()->globalExec();
    const char *methodName = JavaString(mName).characters();
    Value func = imp->get (exec, Identifier (methodName));
    if (func.isNull() || func.type() == UndefinedType) {
        // Maybe throw an exception here?
        return 0;
    }

    // Call the function object.
    ObjectImp *funcImp = static_cast<ObjectImp*>(func.imp());
    Object thisObj = Object(const_cast<ObjectImp*>(imp));
    List argList = listFromJArray(args);
    Value result = funcImp->call (exec, thisObj, argList);

    // Convert and return the result of the function call.
    return convertValueToJObject (exec, result);
}

jobject KJS_JSObject_JSObjectEval (JNIEnv *env, jclass jsClass, jlong nativeJSObject, jstring jurl, jstring jscript, jboolean ctx)
{
    fprintf (stderr, "%s:\n", __PRETTY_FUNCTION__);
    return 0;
}

jobject KJS_JSObject_JSObjectGetMember (JNIEnv *env, jclass jsClass, jlong nativeJSObject, jstring jurl, jstring jname, jboolean ctx)
{
    fprintf (stderr, "%s:\n", __PRETTY_FUNCTION__);
    return 0;
}

void KJS_JSObject_JSObjectSetMember (JNIEnv *env, jclass jsClass, jlong nativeJSObject, jstring jurl, jstring jname, jobject value, jboolean ctx)
{
    fprintf (stderr, "%s:\n", __PRETTY_FUNCTION__);
}

void KJS_JSObject_JSObjectRemoveMember (JNIEnv *env, jclass jsClass, jlong nativeJSObject, jstring jurl, jstring jname, jboolean ctx)
{
    fprintf (stderr, "%s:\n", __PRETTY_FUNCTION__);
}

jobject KJS_JSObject_JSObjectGetSlot (JNIEnv *env, jclass jsClass, jlong nativeJSObject, jstring jurl, jint jindex, jboolean ctx)
{
    fprintf (stderr, "%s:\n", __PRETTY_FUNCTION__);
    return 0;
}

void KJS_JSObject_JSObjectSetSlot (JNIEnv *env, jclass jsClass, jlong nativeJSObject, jstring jurl, jint jindex, jobject value, jboolean ctx)
{
    fprintf (stderr, "%s:\n", __PRETTY_FUNCTION__);
}

jstring KJS_JSObject_JSObjectToString (JNIEnv *env, jclass clazz, jlong nativeJSObject)
{
    fprintf (stderr, "%s:\n", __PRETTY_FUNCTION__);
    return 0;
}

}
