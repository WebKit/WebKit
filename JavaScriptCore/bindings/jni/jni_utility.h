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
#include <JavaVM/jni.h>

const char *getCharactersFromJString (JNIEnv *env, jstring aJString);
void releaseCharactersForJString (JNIEnv *env, jstring aJString, const char *s);

jobject callJNIObjectMethod( jobject obj, const char *name, const char *sig, ... );
void callJNIVoidMethod( jobject obj, const char *name, const char *sig, ... );
jboolean callJNIBooleanMethod( jobject obj, const char *name, const char *sig, ... );
jbyte callJNIByteMethod( jobject obj, const char *name, const char *sig, ... );
jchar callJNICharMethod( jobject obj, const char *name, const char *sig, ... );
jshort callJNIShortMethod( jobject obj, const char *name, const char *sig, ... );
jint callJNIIntMethod( jobject obj, const char *name, const char *sig, ... );
jlong callJNILongMethod( jobject obj, const char *name, const char *sig, ... );
jfloat callJNIFloatMethod( jobject obj, const char *name, const char *sig, ... );
jdouble callJNIDoubleMethod( jobject obj, const char *name, const char *sig, ... );

jobject callJNIObjectMethodA( jobject obj, const char *name, const char *sig, jvalue *args);
void callJNIVoidMethodA( jobject obj, const char *name, const char *sig, jvalue *args);
jboolean callJNIBooleanMethodA( jobject obj, const char *name, const char *sig, jvalue *args);
jbyte callJNIByteMethodA( jobject obj, const char *name, const char *sig, jvalue *args);
jchar callJNICharMethodA( jobject obj, const char *name, const char *sig, jvalue *args);
jshort callJNIShortMethodA( jobject obj, const char *name, const char *sig, jvalue *args);
jint callJNIIntMethodA( jobject obj, const char *name, const char *sig, jvalue *args);
jlong callJNILongMethodA( jobject obj, const char *name, const char *sig, jvalue *args);
jfloat callJNIFloatMethodA( jobject obj, const char *name, const char *sig, jvalue *args);
jdouble callJNIDoubleMethodA( jobject obj, const char *name, const char *sig, jvalue *args);
