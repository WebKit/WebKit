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

gTestfile = 'ToBoolean-001-n.js';

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
var SECTION = "JavaArray to boolean";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
  SECTION;
startTest();

var dt = applet.createQAObject("com.netscape.javascript.qa.liveconnect.DataTypeClass");
var DT = dt; // Should be class itself, but WebKit doesn't support accessing classes.,

var a = new Array();
var i = 0;

// Passing a JavaArray to a method that expects a boolean should
// always convert the array to true

var emptyByteArray = wkTestStringToJavaByteArray("");

a[i++] = new TestObject (
  "dt.setBoolean( emptyByteArray )",
  "dt.PUB_BOOLEAN ",
  "dt.getBoolean() ",
  "typeof dt.getBoolean() ",
  'true',
  '"boolean"' );

a[i++] = new TestObject (
  "dt.setBoolean( DT.staticGetByteArray() )",
  "dt.PUB_BOOLEAN ",
  "dt.getBoolean() ",
  "typeof dt.getBoolean() ",
  'true',
  '"boolean"' );

  a[i++] = new TestObject (
  "dt.setBoolean( DT.staticGetByteArray() )",
  "dt.PUB_BOOLEAN ",
  "dt.getBoolean() ",
  "typeof dt.getBoolean() ",
  'true',
  '"boolean"' );

  a[i++] = new TestObject (
  "dt.setBoolean( DT.staticGetCharArray() )",
  "dt.PUB_BOOLEAN ",
  "dt.getBoolean() ",
  "typeof dt.getBoolean() ",
  'true',
  '"boolean"' );

  a[i++] = new TestObject (
  "dt.setBoolean( DT.staticGetShortArray() )",
  "dt.PUB_BOOLEAN ",
  "dt.getBoolean() ",
  "typeof dt.getBoolean() ",
  'true',
  '"boolean"' );

  a[i++] = new TestObject (
  "dt.setBoolean( DT.staticGetLongArray() )",
  "dt.PUB_BOOLEAN ",
  "dt.getBoolean() ",
  "typeof dt.getBoolean() ",
  'true',
  '"boolean"' );

  a[i++] = new TestObject (
  "dt.setBoolean( DT.staticGetIntArray() )",
  "dt.PUB_BOOLEAN ",
  "dt.getBoolean() ",
  "typeof dt.getBoolean() ",
  'true',
  '"boolean"' );

  a[i++] = new TestObject (
  "dt.setBoolean( DT.staticGetFloatArray() )",
  "dt.PUB_BOOLEAN ",
  "dt.getBoolean() ",
  "typeof dt.getBoolean() ",
  'true',
  '"boolean"' );

  a[i++] = new TestObject (
  "dt.setBoolean( DT.staticGetObjectArray() )",
  "dt.PUB_BOOLEAN ",
  "dt.getBoolean() ",
  "typeof dt.getBoolean() ",
  'true',
  '"boolean"' );

for ( i = 0; i < a.length; i++ ) {
  shouldBeWithErrorCheck(
    a[i].description +"; "+ a[i].javaFieldName,
    a[i].jsValue);

  shouldBeWithErrorCheck(
    a[i].description +"; " + a[i].javaMethodValue,
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
    this.javaTypeValue = typeof this.javaFieldValue;

  this.jsValue   = jsValue;
  this.jsType      = jsType;
}
