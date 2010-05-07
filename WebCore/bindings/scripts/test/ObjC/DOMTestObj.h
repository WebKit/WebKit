/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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

#import <WebCore/DOMObject.h>

#if WEBKIT_VERSION_MAX_ALLOWED >= WEBKIT_VERSION_LATEST

@class DOMTestObj;
@class DOMlog;
@class NSString;
@protocol DOMEventListener;

@interface DOMTestObj : DOMObject
- (int)readOnlyIntAttr;
- (NSString *)readOnlyStringAttr;
- (DOMTestObj *)readOnlyTestObjAttr;
- (int)intAttr;
- (void)setIntAttr:(int)newIntAttr;
- (long long)longLongAttr;
- (void)setLongLongAttr:(long long)newLongLongAttr;
- (unsigned long long)unsignedLongLongAttr;
- (void)setUnsignedLongLongAttr:(unsigned long long)newUnsignedLongLongAttr;
- (NSString *)stringAttr;
- (void)setStringAttr:(NSString *)newStringAttr;
- (DOMTestObj *)testObjAttr;
- (void)setTestObjAttr:(DOMTestObj *)newTestObjAttr;
- (int)attrWithException;
- (void)setAttrWithException:(int)newAttrWithException;
- (int)attrWithSetterException;
- (void)setAttrWithSetterException:(int)newAttrWithSetterException;
- (int)attrWithGetterException;
- (void)setAttrWithGetterException:(int)newAttrWithGetterException;
- (int)customAttr;
- (void)setCustomAttr:(int)newCustomAttr;
- (NSString *)scriptStringAttr;
- (void)voidMethod;
- (void)voidMethodWithArgs:(int)intArg strArg:(NSString *)strArg objArg:(DOMTestObj *)objArg;
- (int)intMethod;
- (int)intMethodWithArgs:(int)intArg strArg:(NSString *)strArg objArg:(DOMTestObj *)objArg;
- (DOMTestObj *)objMethod;
- (DOMTestObj *)objMethodWithArgs:(int)intArg strArg:(NSString *)strArg objArg:(DOMTestObj *)objArg;
- (void)serializedValue:(NSString *)serializedArg;
- (void)methodWithException;
- (void)customMethod;
- (void)customMethodWithArgs:(int)intArg strArg:(NSString *)strArg objArg:(DOMTestObj *)objArg;
- (void)customArgsAndException:(DOMlog *)intArg;
- (void)addEventListener:(NSString *)type listener:(id <DOMEventListener>)listener useCapture:(BOOL)useCapture;
- (void)removeEventListener:(NSString *)type listener:(id <DOMEventListener>)listener useCapture:(BOOL)useCapture;
- (void)withDynamicFrame;
- (void)withDynamicFrameAndArg:(int)intArg;
- (void)withDynamicFrameAndOptionalArg:(int)intArg optionalArg:(int)optionalArg;
- (void)withDynamicFrameAndUserGesture:(int)intArg;
- (void)withDynamicFrameAndUserGestureASAD:(int)intArg optionalArg:(int)optionalArg;
- (void)withScriptStateVoid;
- (DOMTestObj *)withScriptStateObj;
- (void)withScriptStateVoidException;
- (DOMTestObj *)withScriptStateObjException;
- (void)methodWithOptionalArg:(int)opt;
- (void)methodWithNonOptionalArgAndOptionalArg:(int)nonOpt opt:(int)opt;
- (void)methodWithNonOptionalArgAndTwoOptionalArgs:(int)nonOpt opt1:(int)opt1 opt2:(int)opt2;
@end

#endif
