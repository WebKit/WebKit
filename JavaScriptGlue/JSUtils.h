#ifndef JSUtils_h
#define JSUtils_h

/*
    JSUtils.h
*/

#include <JavaScriptGlue/JavaScriptGlue.h>

#include <JavaScriptCore/value.h>
#include <JavaScriptCore/object.h>
#include <JavaScriptCore/types.h>
#include <JavaScriptCore/interpreter.h>
#include <JavaScriptCore/protect.h>
#include <JavaScriptCore/collector.h>
#include <JavaScriptCore/ustring.h>

using namespace KJS;

class JSBase;
class JSUserObject;
class JSRun;
class JSValueWrapper;
class JSUserObjectImp;

UString CFStringToUString(CFStringRef inCFString);
CFStringRef UStringToCFString(const UString& inUString);
Identifier CFStringToIdentifier(CFStringRef inCFString);
CFStringRef IdentifierToCFString(const Identifier& inIdentifier);
JSUserObject *KJSValueToJSObject(JSValue *inValue, ExecState *exec);
CFTypeRef KJSValueToCFType(JSValue *inValue, ExecState *exec);
JSValue *JSObjectKJSValue(JSUserObject* ptr);
CFTypeRef GetCFNull(void);

inline CFTypeRef RetainCFType(CFTypeRef x) { if (x) x = CFRetain(x); return x; }
inline void ReleaseCFType(CFTypeRef x) { if (x) CFRelease(x);  }

enum {
    kJSInvalidTypeID = 0,
    kJSObjectTypeID,
    kJSRunTypeID
};

enum {
    kJSUserObjectDataTypeUnknown,
    kJSUserObjectDataTypeJSValueWrapper,
    kJSUserObjectDataTypeCFType
};


#endif
