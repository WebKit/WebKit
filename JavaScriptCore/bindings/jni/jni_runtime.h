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
#ifndef _JNI_RUNTIME_H_
#define _JNI_RUNTIME_H_

#include <CoreFoundation/CoreFoundation.h>

#include <jni_utility.h>
#include <runtime.h>

namespace KJS
{
class Value;
}

namespace Bindings
{

class JavaString
{
public:
    JavaString () : _env(0), _characters(0), _jString(0) {};
    
    JavaString (JNIEnv *e, jstring s) : _env(e), _jString(s) {
        _characters = getCharactersFromJStringInEnv (_env, s);
    }
    
    ~JavaString () {
        releaseCharactersForJStringInEnv (_env, _jString, _characters);
    }

    JavaString(const JavaString &other) : _env(other._env), _jString (other._jString)
    {
        _characters = getCharactersFromJStringInEnv (_env, _jString);
    }

    JavaString &operator=(const JavaString &other)
    {
        if (this == &other)
            return *this;
            
        releaseCharactersForJStringInEnv (_env, _jString, _characters);
        
        _env = other._env;
        _jString = other._jString;
        _characters = getCharactersFromJStringInEnv (_env, _jString);
        
        return *this;
    }

    const char *characters() { return _characters; }
    
private:
    JNIEnv *_env;
    const char *_characters;
    jstring _jString;
};


class JavaParameter : public Parameter
{
public:
    JavaParameter () : _type (0) {};
    
    JavaParameter (JNIEnv *env, jstring type) {
        _type = new JavaString (env, type);
    };
    
    ~JavaParameter() {
        delete _type;
    };

    JavaParameter(const JavaParameter &other) : Parameter() {
        _type = other._type;
    };

    JavaParameter &operator=(const JavaParameter &other)
    {
        if (this == &other)
            return *this;
            
        delete _type;
        
        _type = other._type;

        return *this;
    }
    
    virtual RuntimeType type() const { return _type->characters(); }
    
private:
    JavaString *_type;
};


class JavaConstructor : public Constructor
{
public:
    JavaConstructor() : _parameters (0), _numParameters(0) {};
    
    JavaConstructor (JNIEnv *e, jobject aConstructor);
    
    ~JavaConstructor() {
        delete [] _parameters;
    };

    void _commonCopy(const JavaConstructor &other) {
        _numParameters = other._numParameters;
        _parameters = new JavaParameter[_numParameters];
        long i;
        for (i = 0; i < _numParameters; i++) {
            _parameters[i] = other._parameters[i];
        }
    }
    
    JavaConstructor(const JavaConstructor &other) : Constructor() {
        _commonCopy (other);
    };

    JavaConstructor &operator=(const JavaConstructor &other)
    {
        if (this == &other)
            return *this;
            
        delete [] _parameters;
        
        _commonCopy (other);

        return *this;
    }

    virtual KJS::Value value() const { return KJS::Value(0); }
    virtual Parameter *parameterAt(long i) const { return &_parameters[i]; };
    virtual long numParameters() const { return _numParameters; };
    
private:
    JavaParameter *_parameters;
    long _numParameters;
};


class JavaField : public Field
{
public:
    JavaField() : _name(0), _type(0) {};
    JavaField (JNIEnv *env, jobject aField);
    ~JavaField() {
        delete _name;
        delete _type;
    };

    JavaField(const JavaField &other) : Field(), _name(other._name), _type(other._type) {};

    JavaField &operator=(const JavaField &other)
    {
        if (this == &other)
            return *this;
            
        delete _name;
        delete _type;
        
        _name = other._name;
        _type = other._type;

        return *this;
    }
    
    virtual KJS::Value value() const { return KJS::Value(0); }
    virtual const char *name() const { return _name->characters(); }
    virtual RuntimeType type() const { return _type->characters(); }
    
private:
    JavaString *_name;
    JavaString *_type;
};


class JavaMethod : public Method
{
public:
    JavaMethod() : Method(), _name(0), _signature(0), _returnType(0) {};
    
    JavaMethod (JNIEnv *env, jobject aMethod);
    
    void _commonDelete() {
        delete _name;
        delete _returnType;
        delete [] _parameters;
    };
    
    ~JavaMethod () {
        _commonDelete();
    };

    void _commonCopy(const JavaMethod &other) {
        _name = other._name;
        _returnType = other._returnType;

        _numParameters = other._numParameters;
        _parameters = new JavaParameter[_numParameters];
        long i;
        for (i = 0; i < _numParameters; i++) {
            _parameters[i] = other._parameters[i];
        }
    };
    
    JavaMethod(const JavaMethod &other) : Method() {
        _commonCopy(other);
    };

    JavaMethod &operator=(const JavaMethod &other)
    {
        if (this == &other)
            return *this;
            
        _commonDelete();
        _commonCopy(other);

        return *this;
    };

    virtual KJS::Value value() const { return KJS::Value(0); }
    virtual const char *name() const { return _name->characters(); };
    virtual const char *signature() const { return _signature->characters(); };
    virtual RuntimeType returnType() const { return _returnType->characters(); };
    virtual Parameter *parameterAt(long i) const { return &_parameters[i]; };
    virtual long numParameters() const { return _numParameters; };
    
private:
    JavaParameter *_parameters;
    long _numParameters;
    JavaString *_name;
    JavaString *_signature;
    JavaString *_returnType;
};

}

#endif
