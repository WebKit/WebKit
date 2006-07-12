// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef JSBase_h
#define JSBase_h

/* JavaScript engine interface */

/*! @typedef JSContextRef A JavaScript execution context. Holds the global object and other execution state. */
typedef struct __JSContext* JSContextRef;
/*! @typedef JSString A UTF16 character buffer. The fundamental string representation in JavaScript. */
typedef struct __JSString* JSStringRef;
/*! @typedef JSClassRef A JavaScript class. Used with JSObjectMake to construct objects with custom behavior. */
typedef struct __JSClass* JSClassRef;
/*! @typedef JSPropertyListRef A JavaScript property list. Used for listing the properties in an object so they can be enumerated. */
typedef struct __JSPropertyList* JSPropertyListRef;
/*! @typedef JSPropertyEnumeratorRef A JavaScript property enumerator. Used for enumerating the properties in an object. */
typedef struct __JSPropertyEnumerator* JSPropertyEnumeratorRef;


/* JavaScript data types */

/*! @typedef JSValueRef A JavaScript value. The base type for all JavaScript values, and polymorphic functions on them. */
typedef const struct __JSValue* JSValueRef;
/*! @typedef JSObjectRef A JavaScript object. A JSObject is a JSValue. */
typedef struct __JSValue* JSObjectRef;

#endif // JSBase_h
