/* ***** BEGIN LICENSE BLOCK *****
 *
 * Copyright (C) 1997, 1998 Netscape Communications Corporation.
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

gTestfile = 'ToDouble-003.js';

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
var SECTION = "number conversion to float";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
  SECTION;
startTest();

var dt = applet.createQAObject("com.netscape.javascript.qa.liveconnect.DataTypeClass");

var a = new Array();
var i = 0;

// passing a number value to a method that expects a float should round JS
// number to float precision.  unrepresentably large values are converted
// to +/- Infinity.

// Special cases:  0, -0, Infinity, -Infinity, and NaN

a[i++] = new TestObject(
  "dt.setFloat( 0 )",
  "dt.PUB_FLOAT",
  "dt.getFloat()",
  "typeof dt.getFloat()",
  '0',
  "'number'" );

a[i++] = new TestObject(
  "dt.setFloat( -0 )",
  "Infinity / dt.PUB_FLOAT",
  "Infinity / dt.getFloat()",
  "typeof dt.getFloat()",
  '-Infinity',
  '"number"' );

a[i++] = new TestObject(
  "dt.setFloat( Infinity )",
  "dt.PUB_FLOAT ",
  "dt.getFloat() ",
  "typeof dt.getFloat()",
  'Infinity',
  '"number"' );

a[i++] = new TestObject(
  "dt.setFloat( -Infinity )",
  "dt.PUB_FLOAT",
  "dt.getFloat() ",
  "typeof dt.getFloat()",
  '-Infinity',
  '"number"' );

a[i++] = new TestObject(
  "dt.setFloat( NaN )",
  "dt.PUB_FLOAT",
  "dt.getFloat()",
  "typeof dt.getFloat()",
  'NaN',
  '"number"' );

a[i++] = new TestObject(
  "dt.setFloat( java.lang.Float.MAX_VALUE )",
  "dt.PUB_FLOAT",
  "dt.getFloat()",
  "typeof dt.getFloat()",
  'java.lang.Float.MAX_VALUE',
  '"number"' );

a[i++] = new TestObject(
  "dt.setFloat( java.lang.Float.MIN_VALUE )",
  "dt.PUB_FLOAT",
  "dt.getFloat()",
  "typeof dt.getFloat()",
  'java.lang.Float.MIN_VALUE',
  '"number"' );

a[i++] = new TestObject(
  "dt.setFloat( -java.lang.Float.MAX_VALUE )",
  "dt.PUB_FLOAT",
  "dt.getFloat()",
  "typeof dt.getFloat()",
  '-java.lang.Float.MAX_VALUE',
  '"number"' );

a[i++] = new TestObject(
  "dt.setFloat( -java.lang.Float.MIN_VALUE )",
  "dt.PUB_FLOAT",
  "dt.getFloat()",
  "typeof dt.getFloat()",
  '-java.lang.Float.MIN_VALUE',
  '"number"' );

a[i++] = new TestObject(
  "dt.setFloat(1.7976931348623157E+308)",
  "dt.PUB_FLOAT",
  "dt.getFloat()",
  "typeof dt.getFloat()",
  'Infinity',
  '"number"' );

a[i++] = new TestObject(
  "dt.setFloat(1.7976931348623158e+308)",
  "dt.PUB_FLOAT",
  "dt.getFloat()",
  "typeof dt.getFloat()",
  'Infinity',
  '"number"' );

a[i++] = new TestObject(
  "dt.setFloat(1.7976931348623159e+308)",
  "dt.PUB_FLOAT",
  "dt.getFloat()",
  "typeof dt.getFloat()",
  'Infinity',
  '"number"' );

a[i++] = new TestObject(
  "dt.setFloat(-1.7976931348623157E+308)",
  "dt.PUB_FLOAT",
  "dt.getFloat()",
  "typeof dt.getFloat()",
  '-Infinity',
  '"number"' );

a[i++] = new TestObject(
  "dt.setFloat(-1.7976931348623158e+308)",
  "dt.PUB_FLOAT",
  "dt.getFloat()",
  "typeof dt.getFloat()",
  '-Infinity',
  '"number"' );

a[i++] = new TestObject(
  "dt.setFloat(-1.7976931348623159e+308)",
  "dt.PUB_FLOAT",
  "dt.getFloat()",
  "typeof dt.getFloat()",
  '-Infinity',
  '"number"' );

a[i++] = new TestObject(
  "dt.setFloat(1e-2000)",
  "dt.PUB_FLOAT",
  "dt.getFloat()",
  "typeof dt.getFloat()",
  '0',
  "'number'" );

a[i++] = new TestObject(
  "dt.setFloat(1e2000)",
  "dt.PUB_FLOAT",
  "dt.getFloat()",
  "typeof dt.getFloat()",
  'Infinity',
  '"number"' );

a[i++] = new TestObject(
  "dt.setFloat(-1e2000)",
  "dt.PUB_FLOAT",
  "dt.getFloat()",
  "typeof dt.getFloat()",
  '-Infinity',
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
