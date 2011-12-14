/*
 * Copyright 2011, The Android Open Source Project
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

#ifndef JavaValueV8_h
#define JavaValueV8_h

#if ENABLE(JAVA_BRIDGE)

#include "JavaType.h"

#include <wtf/text/WTFString.h>

namespace JSC {

namespace Bindings {

class JavaInstance;

// A variant used to represent a Java value, almost identical to the JNI
// jvalue type. It exists because the logic to convert between JavaScript
// objects (as JavaNPObject or JSValue) and Java objects should not depend upon
// JNI, to allow ports to provide a JavaInstance object etc which does not use
// JNI. This means that the 'object' field of this variant uses JavaInstance,
// not jobject.
//
// Note that this class is independent of the JavaScript engine, but is
// currently used only with V8.
// See https://bugs.webkit.org/show_bug.cgi?id=57023.
struct JavaValue {
    JavaValue() : m_type(JavaTypeInvalid) {}

    JavaType m_type;
    // We don't use a union because we want to be able to ref-count some of the
    // values. This requires types with non-trivial constructors.
    RefPtr<JavaInstance> m_objectValue;
    bool m_booleanValue;
    signed char m_byteValue;
    unsigned short m_charValue;
    short m_shortValue;
    int m_intValue;
    long long m_longValue;
    float m_floatValue;
    double m_doubleValue;
    String m_stringValue;
};

} // namespace Bindings

} // namespace JSC

#endif // ENABLE(JAVA_BRIDGE)

#endif // JavaValueV8_h
