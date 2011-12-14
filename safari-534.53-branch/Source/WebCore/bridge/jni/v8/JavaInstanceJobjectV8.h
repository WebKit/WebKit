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

#ifndef JavaInstanceJobjectV8_h
#define JavaInstanceJobjectV8_h

#if ENABLE(JAVA_BRIDGE)

#include "JNIUtility.h"
#include "JavaInstanceV8.h"
#include "JobjectWrapper.h"

#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>

using namespace WTF;

namespace JSC {

namespace Bindings {

class JavaInstanceJobject : public JavaInstance {
public:
    JavaInstanceJobject(jobject instance);

    // JavaInstance implementation
    virtual JavaClass* getClass() const;
    virtual JavaValue invokeMethod(const JavaMethod*, JavaValue* args);
    virtual JavaValue getField(const JavaField*);
    virtual void begin();
    virtual void end();

    jobject javaInstance() const { return m_instance->m_instance; }

protected:
    RefPtr<JobjectWrapper> m_instance;
    mutable OwnPtr<JavaClass> m_class;
};

} // namespace Bindings

} // namespace JSC

#endif // ENABLE(JAVA_BRIDGE)

#endif // JavaInstanceJobjectV8_h
