/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#ifndef JSUtils_h
#define JSUtils_h

#include "JavaScriptGlue.h"

#include <JavaScriptCore/Collector.h>
#include <JavaScriptCore/JSValue.h>
#include <JavaScriptCore/Completion.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/Protect.h>
#include <JavaScriptCore/UString.h>

#include <wtf/RefPtr.h>

using namespace JSC;

class JSBase;
class JSUserObject;
class JSRun;
class JSValueWrapper;
class JSUserObjectImp;

UString CFStringToUString(CFStringRef inCFString);
CFStringRef UStringToCFString(const UString& inUString);
Identifier CFStringToIdentifier(CFStringRef inCFString, ExecState*);
CFStringRef IdentifierToCFString(const Identifier& inIdentifier);
JSUserObject *KJSValueToJSObject(JSValue inValue, ExecState *exec);
CFTypeRef KJSValueToCFType(JSValue inValue, ExecState *exec);
JSValue JSObjectKJSValue(JSUserObject* ptr);
CFTypeRef GetCFNull(void);

inline CFTypeRef RetainCFType(CFTypeRef x) { if (x) x = CFRetain(x); return x; }
inline void ReleaseCFType(CFTypeRef x) { if (x) CFRelease(x);  }

ExecState* getThreadGlobalExecState();

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
