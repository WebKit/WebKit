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

gTestfile = 'ToLong-001.js';

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
var SECTION = "JavaScript Object to int";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
  SECTION;
startTest();

var dt = applet.createQAObject("com.netscape.javascript.qa.liveconnect.DataTypeClass");

var a = new Array();
var i = 0;

// 3.3.6.4 Other JavaScript Objects
// Passing a JavaScript object to a java method that that expects a int
// should:
// 1. Apply the ToPrimitive operator (ECMA 9.3) to the JavaScript object
// with hint Number
// 2. Convert Result(1) to Java numeric type using the rules in 3.3.3.

a[i++] = new TestObject(
  "dt.setLong(new Boolean(true))",
  "dt.PUB_LONG",
  "dt.getLong()",
  "typeof dt.getLong()",
  '1',
  '"number"');

a[i++] = new TestObject(
  "dt.setLong(bool = new Boolean(false))",
  "dt.PUB_LONG",
  "dt.getLong()",
  "typeof dt.getLong()",
  '0',
  '"number"');

var number = new Number(0);

a[i++] = new TestObject(
  "dt.setLong( number )",
  "dt.PUB_LONG",
  "dt.getLong()",
  "typeof dt.getLong()",
  '0',
  '"number"');


a[i++] = new TestObject(
  "dt.setLong(new String(32767))",
  "dt.PUB_LONG",
  "dt.getLong()",
  "typeof dt.getLong()",
  '32767',
  '"number"');

a[i++] = new TestObject(
  "dt.setLong(new String(-32768))",
  "dt.PUB_LONG",
  "dt.getLong()",
  "typeof dt.getLong()",
  '-32768',
  '"number"');

a[i++] = new TestObject(
  "dt.setLong(new MyObject('5.5'))",
  "dt.PUB_LONG",
  "dt.getLong()",
  "typeof dt.getLong()",
  '5',
  '"number"');

a[i++] = new TestObject(
  "dt.setLong(new MyOtherObject('-107.5'))",
  "dt.PUB_LONG",
  "dt.getLong()",
  "typeof dt.getLong()",
  '-107',
  '"number"');

a[i++] = new TestObject(
  "dt.setLong(new AnotherObject('6666'))",
  "dt.PUB_LONG",
  "dt.getLong()",
  "typeof dt.getLong()",
  '6666',
  '"number"');

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

function MyObject( stringValue ) {
  this.stringValue = String(stringValue);
  this.toString = new Function( "return this.stringValue" );
}

function MyOtherObject( value ) {
  this.toString = null;
  this.value = value;
  this.valueOf = new Function( "return this.value" );
}

function AnotherObject( value ) {
  this.toString = new Function( "return new Number(666)" );
  this.value = value;
  this.valueOf = new Function( "return this.value" );
}

function TestObject(description, javaField, javaMethod, javaType, jsValue, jsType)
{
  this.description = description;
  this.javaFieldName = javaField;
  this.javaMethodName = javaMethod;
  this.javaTypeName = javaType,
  this.jsValue = jsValue;
  this.jsType = jsType;
}
