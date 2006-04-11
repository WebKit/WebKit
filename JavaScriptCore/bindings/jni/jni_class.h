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

#ifndef JNI_CLASS_H_
#define JNI_CLASS_H_

#include <jni_runtime.h>

namespace KJS {

namespace Bindings {

class JavaClass : public Class
{
public:
    JavaClass (jobject anInstance);
    ~JavaClass ();

    virtual const char *name() const { return _name; };
    
    virtual MethodList methodsNamed(const char *name, Instance *instance) const;
    
    virtual Field *fieldNamed(const char *name, Instance *instance) const;
    
    virtual Constructor *constructorAt(int i) const {
        return &_constructors[i]; 
    };
    
    virtual int numConstructors() const { return _numConstructors; };
    
    bool isNumberClass() const;
    bool isBooleanClass() const;
    bool isStringClass() const;
    
private:
    JavaClass ();                                 // prevent default construction
    JavaClass (const JavaClass &other);           // prevent copying
    JavaClass &operator=(const JavaClass &other); // prevent copying
    
    const char *_name;
    CFDictionaryRef _fields;
    CFDictionaryRef _methods;
    JavaConstructor *_constructors;
    int _numConstructors;
};

} // namespace Bindings

} // namespace KJS

#endif