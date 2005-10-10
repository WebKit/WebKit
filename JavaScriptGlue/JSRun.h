#ifndef JSRun_h
#define JSRun_h

/*
    JSRun.h
*/

#include "JSBase.h"
#include "JSUtils.h"

class JSInterpreter : public Interpreter {
    public:
        JSInterpreter(ObjectImp *global, JSFlags flags) : Interpreter(global), fJSFlags(flags) { }
        JSInterpreter(ObjectImp *global) : Interpreter(global), fJSFlags(kJSFlagNone) { }
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
        ObjectImp *GlobalObject() const;
        JSInterpreter* GetInterpreter();
        Completion Evaluate();
        bool CheckSyntax();
        JSFlags Flags() const;
    private:
        UString fSource;
        ProtectedPtr<ObjectImp> fGlobalObject;
        JSInterpreter fInterpreter;
        JSFlags fFlags;
};

#endif
