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
#ifndef _RUNTIME_OBJECT_H_
#define _RUNTIME_OBJECT_H_

#include <JavaScriptCore/runtime.h>
#include <JavaScriptCore/object.h>
#include <JavaScriptCore/protect.h>

namespace KJS {

class RuntimeObjectImp : public ObjectImp {
public:
    RuntimeObjectImp(ObjectImp *proto);
    
    ~RuntimeObjectImp();
    
    RuntimeObjectImp(Bindings::Instance *i, bool ownsInstance = true);

    const ClassInfo *classInfo() const { return &info; }

    virtual Value get(ExecState *exec, const Identifier &propertyName) const;

    virtual void put(ExecState *exec, const Identifier &propertyName,
                     const Value &value, int attr = None);

    virtual bool canPut(ExecState *exec, const Identifier &propertyName) const;

    virtual bool hasOwnProperty(ExecState *exec,
			     const Identifier &propertyName) const;


    virtual bool deleteProperty(ExecState *exec,
                                const Identifier &propertyName);

    virtual Value defaultValue(ExecState *exec, Type hint) const;

    void setInternalInstance (Bindings::Instance *i) { instance = i; }
    Bindings::Instance *getInternalInstance() const { return instance; }

    virtual bool implementsCall() const;
    virtual Value call(ExecState *exec, Object &thisObj, const List &args);
    
    static const ClassInfo info;

private:
    Bindings::Instance *instance;
    bool ownsInstance;
};
    
}; // namespace

#endif
