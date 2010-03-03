/* ***** BEGIN LICENSE BLOCK *****
 *
 * Copyright (C) 1998 Netscape Communications Corporation.
 * Copyright (C) 2010 Apple Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * ***** END LICENSE BLOCK ***** */

gTestfile = 'ToByte-001.js';

/* -*- Mode: java; tab-width: 8 -*-
 * Copyright (C) 1997, 1998 Netscape Communications Corporation,
 * All Rights Reserved.
 */

/**
 *  JavaScript to Java type conversion.
 *
 *  This test passes JavaScript number values to several Java methods
 *  that expect arguments of various types, and verifies that the value is
 *  converted to the correct value and type.
 *
 *  This tests instance methods, and not static methods.
 *
 *  Running these tests successfully requires you to have
 *  com.netscape.javascript.qa.liveconnect.DataTypeClass on your classpath.
 *
 *  Specification:  Method Overloading Proposal for Liveconnect 3.0
 *
 *  @author: christine@netscape.com
 *
 */
var SECTION = "number conversion to byte";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
  SECTION;
startTest();

var dt = applet.createQAObject("com.netscape.javascript.qa.liveconnect.DataTypeClass");

var a = new Array();
var i = 0;

// passing a JS number to a method that expects a int:
// round JS number to integral value using round-to-negative-infinity mode
// numbers with a magnitude too large to be represented in the target integral
//  type result in a runtime error.
// Nan converts to 0.

// Special cases:  0, -0, Infinity, -Infinity, and NaN

a[i++] = new TestObject(
  "dt.setByte( 0 )",
  "dt.PUB_BYTE",
  "dt.getByte()",
  "typeof dt.getByte()",
  '0',
  '"number"' );

// 0 can only have a negative sign for floats and doubles

a[i++] = new TestObject(
  "dt.setByte( -0 )",
  "Infinity / dt.PUB_BYTE",
  "Infinity / dt.getByte()",
  "typeof dt.getByte()",
  'Infinity',
  '"number"' );

a[i++] = new TestObject(
  "dt.setByte( java.lang.Byte.MAX_VALUE )",
  "dt.PUB_BYTE",
  "dt.getByte()",
  "typeof dt.getByte()",
  'java.lang.Byte.MAX_VALUE',
  '"number"' );

a[i++] = new TestObject(
  "dt.setByte( java.lang.Byte.MIN_VALUE )",
  "dt.PUB_BYTE",
  "dt.getByte()",
  "typeof dt.getByte()",
  'java.lang.Byte.MIN_VALUE',
  '"number"' );

a[i++] = new TestObject(
  "dt.setByte( -java.lang.Byte.MAX_VALUE )",
  "dt.PUB_BYTE",
  "dt.getByte()",
  "typeof dt.getByte()",
  '-java.lang.Byte.MAX_VALUE',
  '"number"' );

a[i++] = new TestObject(
  "dt.setByte(1e-2000)",
  "dt.PUB_BYTE",
  "dt.getByte()",
  "typeof dt.getByte()",
  '0',
  '"number"' );

a[i++] = new TestObject(
  "dt.setByte(-1e-2000)",
  "dt.PUB_BYTE",
  "dt.getByte()",
  "typeof dt.getByte()",
  '0',
  '"number"' );

for ( i = 0; i < a.length; i++ ) {
  try {
    eval(a[i].jsValue);
  } catch (ex) {
    testFailed(a[i].jsValue + ": " + ex);
    continue;
  }

  shouldBeWithErrorCheck(
    a[i].description +"; "+ a[i].javaFieldName,
    a[i].jsValue);

  shouldBeWithErrorCheck(
    a[i].description +"; " + a[i].javaMethodName,
    a[i].jsValue);

  shouldBeWithErrorCheck(
    a[i].javaTypeName,
    a[i].jsType);
}

function TestObject( description, javaField, javaMethod, javaType,
                     jsValue, jsType )
{
  this.description = description;
  this.javaFieldName = javaField;
  this.javaMethodName = javaMethod;
  this.javaTypeName = javaType,

  this.jsValue   = jsValue;
  this.jsType      = jsType;
}
