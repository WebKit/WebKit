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

gTestfile = 'ToLong-002.js';

/**
 *  Preferred Argument Conversion.
 *
 *  Passing a JavaScript boolean to a Java method should prefer to call
 *  a Java method of the same name that expects a Java boolean.
 *
 */
var SECTION = "Preferred argument conversion:  JavaScript Object to Long";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
  SECTION;
startTest();

var TEST_CLASS = applet.createQAObject("com.netscape.javascript.qa.lc3.jsobject.JSObject_006");

function MyFunction() {
  return "hello";
}
MyFunction.valueOf = new Function( "return 999" );

function MyOtherFunction() {
  return "goodbye";
}
MyOtherFunction.valueOf = null;
MyOtherFunction.toString = new Function( "return 999" );

function MyObject(value) {
  this.value = value;
  this.valueOf = new Function("return this.value");
}

function MyOtherObject(stringValue) {
  this.stringValue = String( stringValue );
  this.toString = new Function( "return this.stringValue" );
  this.valueOf = null;
}

function AnotherObject( value ) {
  this.value = value;
  this.valueOf = new Function( "return this.value" );
  this.toString = new Function( "return 666" );
}

// should pass MyFunction.valueOf() to ambiguous

shouldBeWithErrorCheck(
  "TEST_CLASS.ambiguous( MyFunction ) +''",
  "'LONG'");

// should pass MyOtherFunction.toString() to ambiguous

shouldBeWithErrorCheck(
  "TEST_CLASS.ambiguous( MyOtherFunction ) +''",
  "'LONG'");

// should pass MyObject.valueOf() to ambiguous

shouldBeWithErrorCheck(
  "TEST_CLASS.ambiguous( new MyObject(12345) ) +''",
  "'LONG'");

// should pass MyOtherObject.toString() to ambiguous

shouldBeWithErrorCheck(
  "TEST_CLASS.ambiguous( new MyOtherObject(\"12345\") ) +''",
  "'LONG'");

// should pass AnotherObject.valueOf() to ambiguous

shouldBeWithErrorCheck(
  "TEST_CLASS.ambiguous( new AnotherObject(\"12345\") ) +''",
  "'LONG'");

