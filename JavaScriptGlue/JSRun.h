#ifndef JSRun_h
#define JSRun_h

/*
    JSRun.h
*/

#include "JSBase.h"
#include "JSUtils.h"

class JSInterpreter : public Interpreter {
    public:
        JSInterpreter(JSObject *global, JSFlags flags) : Interpreter(global), fJSFlags(flags) { }
        JSInterpreter(JSObject *global) : Interpreter(global), fJSFlags(kJSFlagNone) { }
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
        JSObject *GlobalObject() const;
        JSInterpreter* GetInterpreter();
        Completion Evaluate();
        bool CheckSyntax();
        JSFlags Flags() const;
    private:
        UString fSource;
        ProtectedPtr<JSObject> fGlobalObject;
        JSInterpreter fInterpreter;
        JSFlags fFlags;
};

#endif
