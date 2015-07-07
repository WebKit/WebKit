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

gTestfile = 'ToLong-001.js';

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

function MyObject( value ) {
  this.value = value;
  this.valueOf = new Function( "return this.value" );
}

function MyFunction() {
  return;
}
MyFunction.valueOf = new Function( "return 6060842" );

shouldBeWithErrorCheck(
  "TEST_CLASS.ambiguous( new String() ) +''",
  "'LONG'");

shouldBeWithErrorCheck(
  "TEST_CLASS.ambiguous( new Boolean() ) +''",
  "'LONG'");

shouldBeWithErrorCheck(
  "TEST_CLASS.ambiguous( new Number() ) +''",
  "'LONG'");

shouldBeWithErrorCheck(
  "TEST_CLASS.ambiguous( new Date(0) ) +''",
  "'LONG'");

shouldBeWithErrorCheck(
  "TEST_CLASS.ambiguous( new MyObject(999) ) +''",
  "'LONG'");

shouldBeWithErrorCheck(
  "TEST_CLASS.ambiguous( MyFunction ) +''",
  "'LONG'");


