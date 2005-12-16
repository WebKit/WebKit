//
// JSRun.h
//

#include "JSRun.h"

JSRun::JSRun(CFStringRef source, JSFlags inFlags)
    :   JSBase(kJSRunTypeID),
        fSource(CFStringToUString(source)),
        fGlobalObject(new JSObject()),
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

JSObject *JSRun::GlobalObject() const
{
    return fGlobalObject;
}

JSInterpreter* JSRun::GetInterpreter()
{
    return &fInterpreter;
}

Completion JSRun::Evaluate()
{
    return fInterpreter.evaluate(UString(), 0, fSource.data(), fSource.size());
}

bool JSRun::CheckSyntax()
{
    return fInterpreter.checkSyntax(fSource);
}
