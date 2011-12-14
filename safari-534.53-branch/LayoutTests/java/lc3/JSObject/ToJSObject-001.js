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

gTestfile = 'ToJSObject-001.js';

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
var SECTION = "JavaScript Object to java.lang.String";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
  SECTION;
startTest();

var jsoc = applet.createQAObject("com.netscape.javascript.qa.liveconnect.JSObjectConversion");

var a = new Array();
var i = 0;

// 3.3.6.4 Other JavaScript Objects
// Passing a JavaScript object to a java method that that expects a JSObject
// should wrap the JS object in a new instance of netscape.javascript.JSObject.
// HOwever, since we are running the test from JavaScript, getting the value
// back should return the unwrapped object.

var string  = new String("JavaScript String Value");
/*
a[i++] = new TestObject(
  "jsoc.setJSObject(string)",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  'string',
  'String');

var myobject = new MyObject( string );

a[i++] = new TestObject(
  "jsoc.setJSObject( myobject )",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  'myobject',
  'MyObject');

var bool = new Boolean(true);

a[i++] = new TestObject(
  "jsoc.setJSObject( bool )",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  'bool',
  'Boolean');

bool = new Boolean(false);

a[i++] = new TestObject(
  "jsoc.setJSObject( bool )",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  'bool',
  'Boolean');

var object = new Object();

a[i++] = new TestObject(
  "jsoc.setJSObject( object)",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  'object',
  'Object');

var number = new Number(0);

a[i++] = new TestObject(
  "jsoc.setJSObject( number )",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  'number',
  'Number');

nan = new Number(NaN);

a[i++] = new TestObject(
  "jsoc.setJSObject( nan )",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  'nan',
  'Number');

infinity = new Number(Infinity);

a[i++] = new TestObject(
  "jsoc.setJSObject( infinity )",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  'infinity',
  'Number');

var neg_infinity = new Number(-Infinity);

a[i++] = new TestObject(
  "jsoc.setJSObject( neg_infinity)",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  'neg_infinity',
  'Number');
*/
var array = new Array(1,2,3)

  a[i++] = new TestObject(
    "jsoc.setJSObject(array)",
    "jsoc.PUB_JSOBJECT",
    "jsoc.getJSObject()",
    "jsoc.getJSObject().constructor",
    'array',
    'Array');


a[i++] = new TestObject(
  "jsoc.setJSObject( MyObject )",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  'MyObject',
  'Function');

var THIS = this;

a[i++] = new TestObject(
  "jsoc.setJSObject( THIS )",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  'this',
  'this.constructor');

a[i++] = new TestObject(
  "jsoc.setJSObject( Math )",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  'Math',
  'Object');

a[i++] = new TestObject(
  "jsoc.setJSObject( Function )",
  "jsoc.PUB_JSOBJECT",
  "jsoc.getJSObject()",
  "jsoc.getJSObject().constructor",
  'Function',
  'Function');

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
