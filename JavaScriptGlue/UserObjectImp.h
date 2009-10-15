/*
 * Copyright (C) 2005, 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UserObjectImp_h
#define UserObjectImp_h

#include "JSUtils.h"
#include "JSBase.h"
#include "JSObject.h"
#include <JavaScriptCore/JSType.h>

class UserObjectImp : public JSObject {
public:
    UserObjectImp(PassRefPtr<Structure>, JSUserObject*);
    virtual ~UserObjectImp();

    virtual const ClassInfo *classInfo() const;
    static const ClassInfo info;

    virtual CallType getCallData(CallData&);

    virtual void getOwnPropertyNames(ExecState*, PropertyNameArray&);

    virtual JSValue callAsFunction(ExecState *exec, JSObject *thisObj, const ArgList &args);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue value, PutPropertySlot&);

    JSValue toPrimitive(ExecState*, JSType preferredType = UnspecifiedType) const;
    virtual bool toBoolean(ExecState *exec) const;
    virtual double toNumber(ExecState *exec) const;
    virtual UString toString(ExecState *exec) const;

    virtual void markChildren(MarkStack&);

    JSUserObject *GetJSUserObject() const;

    static PassRefPtr<Structure> createStructure(JSValue prototype)
    {
        return Structure::create(prototype, TypeInfo(ObjectType, OverridesGetOwnPropertySlot | OverridesMarkChildren));
    }

private:
    static JSValue userObjectGetter(ExecState*, const Identifier& propertyName, const PropertySlot&);

    JSUserObject* fJSUserObject;
};

#endif
