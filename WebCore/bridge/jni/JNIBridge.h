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

#ifndef JNIBridge_h
#define JNIBridge_h

#if ENABLE(MAC_JAVA_BRIDGE)

#include "JNIUtility.h"
#include "JavaInstanceJSC.h"

#if USE(JSC)
#include "JavaStringJSC.h"
#endif

namespace JSC
{

namespace Bindings
{

typedef const char* RuntimeType;

class JavaString
{
public:
    JavaString()
    {
        m_impl.init();
    }

    JavaString(JNIEnv* e, jstring s)
    {
        m_impl.init(e, s);
    }

    JavaString(jstring s)
    {
        m_impl.init(getJNIEnv(), s);
    }

    const char* UTF8String() const { return m_impl.UTF8String(); }
    const jchar* uchars() const { return m_impl.uchars(); }
    int length() const { return m_impl.length(); }
#if USE(JSC)
    operator UString() const { return m_impl.uString(); }
#endif

private:
    JavaStringImpl m_impl;
};

class JavaParameter
{
public:
    JavaParameter () : _JNIType(invalid_type) {};
    JavaParameter (JNIEnv *env, jstring type);
    virtual ~JavaParameter() { }

    RuntimeType type() const { return _type.UTF8String(); }
    JNIType getJNIType() const { return _JNIType; }
    
private:
    JavaString _type;
    JNIType _JNIType;
};


class JavaField : public Field
{
public:
    JavaField (JNIEnv *env, jobject aField);

    virtual JSValue valueFromInstance(ExecState *exec, const Instance *instance) const;
    virtual void setValueToInstance(ExecState *exec, const Instance *instance, JSValue aValue) const;
    
    const JavaString& name() const { return _name; }
    virtual RuntimeType type() const { return _type.UTF8String(); }

    JNIType getJNIType() const { return _JNIType; }
    
private:
    void dispatchSetValueToInstance(ExecState *exec, const JavaInstance *instance, jvalue javaValue, const char *name, const char *sig) const;
    jvalue dispatchValueFromInstance(ExecState *exec, const JavaInstance *instance, const char *name, const char *sig, JNIType returnType) const;

    JavaString _name;
    JavaString _type;
    JNIType _JNIType;
    RefPtr<JObjectWrapper> _field;
};


class JavaMethod : public Method
{
public:
    JavaMethod(JNIEnv* env, jobject aMethod);
    ~JavaMethod();

    const JavaString& name() const { return _name; }
    RuntimeType returnType() const { return _returnType.UTF8String(); };
    JavaParameter* parameterAt(int i) const { return &_parameters[i]; };
    int numParameters() const { return _numParameters; };
    
    const char *signature() const;
    JNIType JNIReturnType() const;

    jmethodID methodID (jobject obj) const;
    
    bool isStatic() const { return _isStatic; }

private:
    JavaParameter* _parameters;
    int _numParameters;
    JavaString _name;
    mutable char* _signature;
    JavaString _returnType;
    JNIType _JNIReturnType;
    mutable jmethodID _methodID;
    bool _isStatic;
};

class JavaArray : public Array
{
public:
    JavaArray(jobject array, const char* type, PassRefPtr<RootObject>);
    virtual ~JavaArray();

    RootObject* rootObject() const;

    virtual void setValueAt(ExecState *exec, unsigned int index, JSValue aValue) const;
    virtual JSValue valueAt(ExecState *exec, unsigned int index) const;
    virtual unsigned int getLength() const;
    
    jobject javaArray() const { return _array->m_instance; }

    static JSValue convertJObjectToArray (ExecState* exec, jobject anObject, const char* type, PassRefPtr<RootObject>);

private:
    RefPtr<JObjectWrapper> _array;
    unsigned int _length;
    const char *_type;
};

} // namespace Bindings

} // namespace JSC

#endif // ENABLE(MAC_JAVA_BRIDGE)

#endif // JNIBridge_h
