#ifndef __JSRun_h
#define __JSRun_h

/*
	JSRun.h
*/

#include "JSBase.h"
#include "JSUtils.h"

class JSInterpreter : public Interpreter {
	public:
		JSInterpreter(const Object &global, JSFlags flags) : Interpreter(global), fJSFlags(flags) { }
		JSInterpreter(const Object &global) : Interpreter(global), fJSFlags(kJSFlagNone) { }
		JSInterpreter() : Interpreter(), fJSFlags(kJSFlagNone) { }
		JSInterpreter::~JSInterpreter() { }
		JSFlags Flags() const { return fJSFlags; }
	private:
		JSFlags fJSFlags;
};

class JSRun : public JSBase {
	public:
		JSRun(CFStringRef source, JSFlags inFlags);
		virtual ~JSRun();

		UString GetSource() const;
		Object GlobalObject() const;
		JSInterpreter* GetInterpreter();
		Completion Evaluate();
		bool CheckSyntax();
		JSFlags Flags() const;
	private:
		UString fSource;		
		ProtectedObject fGlobalObject;
		JSInterpreter fInterpreter;
		JSFlags fFlags;
};

#endif