#ifndef __JSUtils_h
#define __JSUtils_h

/*
	JSUtils.h
*/

#ifndef __CORESERVICES__
#include <CoreServices/CoreServices.h>
#endif

#include <JavaScriptGlue/JavaScriptGlue.h>

#ifdef USE_JSHACK
#include <JSHack/value.h>
#include <JSHack/object.h>
#include <JSHack/types.h>
#include <JSHack/interpreter.h>
#include <JSHack/ustring.h>
#else
#include <JavaScriptCore/value.h>
#include <JavaScriptCore/object.h>
#include <JavaScriptCore/types.h>
#include <JavaScriptCore/interpreter.h>
#include <JavaScriptCore/collector.h>
#include <JavaScriptCore/ustring.h>
#endif

#define JAG_PINK_OR_LATER	1 /* %%% turn on for new JavaScriptCore */

using namespace KJS;

class JSBase;
class JSUserObject;
class JSRun;
class JSValueWrapper;
class JSUserObjectImp;

UString CFStringToUString(CFStringRef inCFString);
CFStringRef UStringToCFString(const UString& inUString);
#if JAG_PINK_OR_LATER
Identifier CFStringToIdentifier(CFStringRef inCFString);
CFStringRef IdentifierToCFString(const Identifier& inIdentifier);
#endif
JSUserObject* KJSValueToJSObject(const Value& inValue, ExecState *exec);
CFTypeRef KJSValueToCFType(const Value& inValue, ExecState *exec);
Value JSObjectKJSValue(JSUserObject* ptr);
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
