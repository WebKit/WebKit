// -*- mode: c++; c-basic-offset: 4 -*-

#ifndef __UserObjectImp_h
#define __UserObjectImp_h

/*
	UserObjectImp.h
*/

#include "JSUtils.h"
#include "JSBase.h"
#include "JSObject.h"

class UserObjectImp : public ObjectImp
{
public:
    UserObjectImp(JSUserObject* userObject);
    virtual ~UserObjectImp();
    
    virtual const ClassInfo *classInfo() const;
    static const ClassInfo info;
    
    virtual bool implementsCall() const;
    
    virtual void getPropertyNames(ExecState *exec, IdentifierSequencedSet& propertyNames);
    
    virtual ValueImp *callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    virtual void put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr = None);
    
    ValueImp *toPrimitive(ExecState *exec, Type preferredType = UnspecifiedType) const;
    virtual bool toBoolean(ExecState *exec) const;
    virtual double toNumber(ExecState *exec) const;
    virtual UString toString(ExecState *exec) const;
    
    virtual void mark();
    
    JSUserObject* GetJSUserObject() const;
protected:
    UserObjectImp();
private:
    static ValueImp *userObjectGetter(ExecState *, const Identifier&, const PropertySlot&);

    JSUserObject* fJSUserObject;
};


#endif
