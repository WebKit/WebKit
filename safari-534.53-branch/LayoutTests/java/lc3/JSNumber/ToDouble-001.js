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

gTestfile = 'ToDouble-001.js';

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
var SECTION = "number conversion";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
  SECTION;
startTest();

var dt = applet.createQAObject("com.netscape.javascript.qa.liveconnect.DataTypeClass");

var a = new Array();
var i = 0;

// passing a number to a java method that expects a double should
// transfer the exact value to java with no rounding or loss of magnitude
// or sign.

// Special cases:  0, -0, Infinity, -Infinity, and NaN

a[i++] = new TestObject(
  "dt.setDouble( 0 )",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '0',
  "'number'" );

a[i++] = new TestObject(
  "dt.setDouble( -0 )",
  "Infinity / dt.PUB_DOUBLE",
  "Infinity / dt.getDouble()",
  "typeof dt.getDouble()",
  '-Infinity',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble( Infinity )",
  "dt.PUB_DOUBLE ",
  "dt.getDouble() ",
  "typeof dt.getDouble()",
  'Infinity',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble( -Infinity )",
  "dt.PUB_DOUBLE",
  "dt.getDouble() ",
  "typeof dt.getDouble()",
  '-Infinity',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble( NaN )",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  'NaN',
  '"number"' );

// test cases from waldemar

a[i++] = new TestObject(
  "dt.setDouble(077777777777777777)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '2251799813685247',
  '"number"' );


a[i++] = new TestObject(
  "dt.setDouble(077777777777777776)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '2251799813685246',
  '"number"' );


a[i++] = new TestObject(
  "dt.setDouble(0x1fffffffffffff)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '9007199254740991',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble(0x20000000000000)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '9007199254740992',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble(0x20123456789abc)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '9027215253084860',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble(0x20123456789abd)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '9027215253084860',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble(0x20123456789abe)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '9027215253084862',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble(0x20123456789abf)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '9027215253084864',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble(0x1000000000000080)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '1152921504606847000',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble(0x1000000000000081)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '1152921504606847200',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble(0x1000000000000100)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '1152921504606847200',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble(0x100000000000017f)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '1152921504606847200',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble(0x1000000000000180)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '1152921504606847500',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble(0x1000000000000181)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '1152921504606847500',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble(1.7976931348623157E+308)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '1.7976931348623157e+308',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble(1.7976931348623158e+308)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '1.7976931348623157e+308',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble(1.7976931348623159e+308)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  'Infinity',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble(-1.7976931348623157E+308)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '-1.7976931348623157e+308',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble(-1.7976931348623158e+308)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '-1.7976931348623157e+308',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble(-1.7976931348623159e+308)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '-Infinity',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble(1e-2000)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '0',
  "'number'" );

a[i++] = new TestObject(
  "dt.setDouble(1e2000)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '1e2000',
  '"number"' );

a[i++] = new TestObject(
  "dt.setDouble(-1e2000)",
  "dt.PUB_DOUBLE",
  "dt.getDouble()",
  "typeof dt.getDouble()",
  '-1e2000',
  '"number"' );

for ( i = 0; i < a.length; i++ ) {
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
