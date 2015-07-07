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

gTestfile = 'ToString-001.js';

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
var SECTION = "JavaArray to String";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
  SECTION;
startTest();

var dt = applet.createQAObject("com.netscape.javascript.qa.liveconnect.DataTypeClass");
var DT = dt; // Should be class itself, but WebKit doesn't support accessing classes.,

var a = new Array();
var i = 0;

// Passing a JavaArray to a method that expects a java.lang.String should
// call the unwrapped array's toString method and return the result as a
// new java.lang.String.

// this should return the byte array string representation, which includes
// the object's hash code

a[i++] = new TestObject (
  "dt.setStringObject( DT.staticGetByteArray() )",
  "dt.PUB_STRING +''",
  "dt.getStringObject() +''",
  "typeof dt.getStringObject() +''",
  'DT.PUB_STATIC_ARRAY_BYTE +""',
  '"string"' );


a[i++] = new TestObject (
  "dt.setStringObject( DT.staticGetCharArray() )",
  "dt.PUB_STRING +''",
  "dt.getStringObject() +''",
  "typeof dt.getStringObject() +''",
  'DT.PUB_STATIC_ARRAY_CHAR +""',
  '"string"' );

a[i++] = new TestObject (
  "dt.setStringObject( DT.staticGetShortArray() )",
  "dt.PUB_STRING +''",
  "dt.getStringObject() +''",
  "typeof dt.getStringObject() +''",
  'DT.PUB_STATIC_ARRAY_SHORT +""',
  '"string"' );

a[i++] = new TestObject (
  "dt.setStringObject( DT.staticGetLongArray() )",
  "dt.PUB_STRING +''",
  "dt.getStringObject() +''",
  "typeof dt.getStringObject() +''",
  'DT.PUB_STATIC_ARRAY_LONG +""',
  '"string"' );

a[i++] = new TestObject (
  "dt.setStringObject( DT.staticGetIntArray() )",
  "dt.PUB_STRING +''",
  "dt.getStringObject() +''",
  "typeof dt.getStringObject() +''",
  'DT.PUB_STATIC_ARRAY_INT +""',
  '"string"' );

a[i++] = new TestObject (
  "dt.setStringObject( DT.staticGetFloatArray() )",
  "dt.PUB_STRING +''",
  "dt.getStringObject() +''",
  "typeof dt.getStringObject() +''",
  'DT.PUB_STATIC_ARRAY_FLOAT +""',
  '"string"' );

a[i++] = new TestObject (
  "dt.setStringObject( DT.staticGetObjectArray() )",
  "dt.PUB_STRING +''",
  "dt.getStringObject() +''",
  "typeof dt.getStringObject() +''",
  'DT.PUB_STATIC_ARRAY_OBJECT +""',
  '"string"' );

a[i++] = new TestObject (
  "dt.setStringObject(java.lang.String(wkTestStringToJavaByteArray(\"hello\")))",
  "dt.PUB_STRING +''",
  "dt.getStringObject() +''",
  "typeof dt.getStringObject() +''",
  '"hello"',
  '"string"' );

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
