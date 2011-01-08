/*
 * Copyright (C) 2003, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright 2010, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef JavaInstanceV8_h
#define JavaInstanceV8_h

#include "npruntime.h"

#include <JavaVM/jni.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

using namespace WTF;

namespace JSC {

namespace Bindings {

class JavaClass;

class JObjectWrapper {
friend class RefPtr<JObjectWrapper>;
friend class JavaField;
friend class JavaInstance;

public:
    jobject instance() const { return m_instance; }
    void setInstance(jobject instance) { m_instance = instance; }

    void ref() { m_refCount++; }
    void deref()
    {
        if (!(--m_refCount))
            delete this;
    }

protected:
    JObjectWrapper(jobject);
    ~JObjectWrapper();

    jobject m_instance;

private:
    JNIEnv* m_env;
    unsigned int m_refCount;
};

class JavaInstance : public RefCounted<JavaInstance> {
public:
    JavaInstance(jobject instance);
    virtual ~JavaInstance();

    JavaClass* getClass() const;

    bool invokeMethod(const char* name, const NPVariant* args, int argsCount, NPVariant* result);

    jobject javaInstance() const { return m_instance->m_instance; }

    // These functions are called before and after the main entry points into
    // the native implementations.  They can be used to establish and cleanup
    // any needed state.
    void begin() { virtualBegin(); }
    void end() { virtualEnd(); }

protected:
    RefPtr<JObjectWrapper> m_instance;
    mutable JavaClass* m_class;

    virtual void virtualBegin();
    virtual void virtualEnd();
};

} // namespace Bindings

} // namespace JSC

#endif // JavaInstanceV8_h
