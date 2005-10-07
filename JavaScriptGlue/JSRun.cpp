//
// JSRun.h
//

#include "JSRun.h"

JSRun::JSRun(CFStringRef source, JSFlags inFlags) 
	:	JSBase(kJSRunTypeID), 
		fSource(CFStringToUString(source)), 
		fGlobalObject(new ObjectImp()),
		fInterpreter(fGlobalObject, inFlags),
		fFlags(inFlags)
{
}
				
JSRun::~JSRun() 
{ 
}

JSFlags JSRun::Flags() const
{
	return fFlags;
}

UString JSRun::GetSource() const
{
	return fSource;
}

ObjectImp *JSRun::GlobalObject() const
{
	return fGlobalObject;
}

JSInterpreter* JSRun::GetInterpreter() 
{
	return &fInterpreter; 
}

Completion JSRun::Evaluate() 
{ 
	return fInterpreter.evaluate(fSource); 
}


bool JSRun::CheckSyntax() 
{ 
	return fInterpreter.checkSyntax(fSource); 
}
