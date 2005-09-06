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
        virtual			~UserObjectImp();
    
        virtual	const ClassInfo	*classInfo() const;
        static	const ClassInfo	info;
        
        virtual bool	implementsCall() const;

		virtual bool hasProperty(ExecState *exec, const Identifier &propertyName) const;
		virtual ReferenceList UserObjectImp::propList(ExecState *exec, bool recursive=true);

        virtual Value	call(ExecState *exec, Object &thisObj, const List &args);
#if JAG_PINK_OR_LATER
		virtual Value	get(ExecState *exec, const Identifier &propertyName) const;
		virtual void	put(ExecState *exec, const Identifier &propertyName, const Value &value, int attr = None);
#else
        virtual Value	get(ExecState* exec, const UString& propertyName) const;
        virtual void	put(ExecState* exec, const UString& propertyName, const Value& value, int attr = None);
#endif

		virtual Value toPrimitive(ExecState *exec, Type preferredType = UnspecifiedType) const;
		virtual bool toBoolean(ExecState *exec) const;
		virtual double toNumber(ExecState *exec) const;
		virtual UString toString(ExecState *exec) const;

        
		virtual void mark();
		
        JSUserObject* GetJSUserObject() const;
	protected:
		UserObjectImp();
    private:
        JSUserObject* fJSUserObject;
};


#endif