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

#include <jni_jsobject.h>

static KJSFindObjectForNativeHandleFunctionPtr findObjectForNativeHandleFunctionPtr = 0;

void KJS_setFindObjectForNativeHandleFunction(KJSFindObjectForNativeHandleFunctionPtr aFunc)
{
    findObjectForNativeHandleFunctionPtr = aFunc;
}

KJSFindObjectForNativeHandleFunctionPtr KJS_findObjectForNativeHandleFunction()
{
    return findObjectForNativeHandleFunctionPtr;
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

static CFMutableDictionaryRef referencesByOwnerDictionary = 0;

static CFMutableDictionaryRef getReferencesByOwnerDictionary()
{
    if (!referencesByOwnerDictionary)
        referencesByOwnerDictionary = CFDictionaryCreateMutable(NULL, 0, NULL, &kCFTypeDictionaryValueCallBacks);
    return referencesByOwnerDictionary;
}

static CFMutableDictionaryRef getReferencesDictionary(const void *owner)
{
    CFMutableDictionaryRef refsByOwner = getReferencesByOwnerDictionary();
    CFMutableDictionaryRef referencesDictionary = 0;
    
    referencesDictionary = (CFMutableDictionaryRef)CFDictionaryGetValue (refsByOwner, owner);
    if (!referencesDictionary) {
        referencesDictionary = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
        CFDictionaryAddValue (refsByOwner, owner, referencesDictionary);
        CFRelease (referencesDictionary);
    }
    return referencesDictionary;
}

// Scan all the dictionary for all the owners to see if any have a 
// reference to the imp, and if so, return it's reference count
// dictionary.
// FIXME:  This is a potential performance bottleneck.
static CFMutableDictionaryRef findReferenceDictionary(KJS::ValueImp *imp)
{
    CFMutableDictionaryRef refsByOwner = getReferencesByOwnerDictionary ();
    CFMutableDictionaryRef referencesDictionary, foundDictionary = 0;
    
    if (refsByOwner) {
        const void **allValues;
        CFIndex count, i;
        
        count = CFDictionaryGetCount(referencesDictionary);
        CFDictionaryGetKeysAndValues (referencesDictionary, NULL, allValues);
        allValues = (const void **)malloc (sizeof(void *) * count);
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

static void addJavaReference (const void *owner, KJS::ValueImp *imp)
{
    CFMutableDictionaryRef referencesDictionary = getReferencesDictionary (owner);
    
    unsigned int numReferences = (unsigned int)CFDictionaryGetValue (referencesDictionary, imp);
    if (numReferences == 0) {
        imp->ref();
        CFDictionaryAddValue (referencesDictionary, imp,  (const void *)1);
    }
    else {
        CFDictionaryReplaceValue (referencesDictionary, imp, (const void *)(numReferences+1));
    }
}

static void removeJavaReference (KJS::ValueImp *imp)
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

// Must be called when the applet is shutdown.
void removeAllJavaReferencesForOwner (const void *owner)
{
    CFMutableDictionaryRef referencesDictionary = getReferencesDictionary (owner);
    
    if (referencesDictionary) {
        void **allImps;
        CFIndex count, i;
        
        count = CFDictionaryGetCount(referencesDictionary);
        CFDictionaryGetKeysAndValues (referencesDictionary, (const void **)allImps, NULL);
        allImps = (void **)malloc (sizeof(void *) * count);
        for(i = 0; i < count; i++) {
            KJS::ValueImp *anImp = static_cast<KJS::ValueImp*>(allImps[i]);
            anImp->deref();
        }
        free ((void *)allImps);
        CFDictionaryRemoveAllValues (referencesDictionary);

        CFMutableDictionaryRef refsByOwner = getReferencesByOwnerDictionary();
        CFDictionaryRemoveValue (refsByOwner, owner);
    }
}


extern "C" {

jlong KJS_JSCreateNativeJSObject (JNIEnv *env, jclass clazz, jstring jurl, jlong nativeHandle, jboolean ctx)
{
    fprintf (stderr, "%s: nativeHandle = %p\n", __PRETTY_FUNCTION__, jlong_to_ptr(nativeHandle));
    
    KJSFindObjectForNativeHandleFunctionPtr aFunc = KJS_findObjectForNativeHandleFunction();
    if (aFunc) {
        KJS::ObjectImp *imp = aFunc(jlong_to_ptr(nativeHandle));
        addJavaReference (jlong_to_ptr(nativeHandle), imp);        
        return ptr_to_jlong(imp);
    }
    
    fprintf (stderr, "%s: unable to find window for nativeHandle = %p\n", __PRETTY_FUNCTION__, jlong_to_ptr(nativeHandle));

    return ptr_to_jlong(0);
}

void KJS_JSObject_JSFinalize (JNIEnv *env, jclass jsClass, jlong nativeJSObject)
{
    removeJavaReference (jlong_to_impptr(nativeJSObject));
    fprintf (stderr, "%s:\n", __PRETTY_FUNCTION__);
}

jobject KJS_JSObject_JSObjectCall (JNIEnv *env, jclass jsClass, jlong nativeJSObject, jstring jurl, jstring methodName, jobjectArray args, jboolean ctx)
{
    fprintf (stderr, "%s:\n", __PRETTY_FUNCTION__);
    return 0;
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
