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

gTestfile = 'array-001.js';

/**
 *  Verify that for-in loops can be used with java objects.
 *
 *  Java array members should be enumerated in for... in loops.
 *
 *
 */
var SECTION = "array-001";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0:  for ... in java objects";
SECTION;
startTest();

// we just need to know the names of all the expected enumerated
// properties.  we will get the values to the original objects.

// for arrays, we just need to know the length, since java arrays
// don't have any extra properties


var dt = applet.createQAObject("com.netscape.javascript.qa.liveconnect.DataTypeClass");
var a = new Array();

a[a.length] = new TestObject(
  wkTestStringToJavaByteArray("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"),
  36 );

try {
    a[a.length] = new TestObject(
      dt.PUB_ARRAY_SHORT,
      dt.PUB_ARRAY_SHORT.length );
} catch (ex) {
    testFailed(ex.toString());
}

try {
    a[a.length] = new TestObject(
      dt.PUB_ARRAY_LONG,
      dt.PUB_ARRAY_LONG.length );
} catch (ex) {
    testFailed(ex.toString());
}

try {
    a[a.length] = new TestObject(
      dt.PUB_ARRAY_DOUBLE,
      dt.PUB_ARRAY_DOUBLE.length );
} catch (ex) {
    testFailed(ex.toString());
}

try {
    a[a.length] = new TestObject(
      dt.PUB_ARRAY_BYTE,
      dt.PUB_ARRAY_BYTE.length );
} catch (ex) {
    testFailed(ex.toString());
}

try {
    a[a.length] = new TestObject(
      dt.PUB_ARRAY_CHAR,
      dt.PUB_ARRAY_CHAR.length );
} catch (ex) {
    testFailed(ex.toString());
}

try {
    a[a.length] = new TestObject(
      dt.PUB_ARRAY_OBJECT,
      dt.PUB_ARRAY_OBJECT.length );
} catch (ex) {
    testFailed(ex.toString());
}

for ( var i = 0; i < a.length; i++ ) {
  // check the number of properties of the enumerated object
  shouldBeWithErrorCheck("a[" + i + "].items", "a[" + i + "].enumedArray.pCount");

  for ( var arrayItem = 0; arrayItem < a[i].items; arrayItem++ )
    shouldBeWithErrorCheck("a[" + i + "].javaArray[" + arrayItem + "]", "a[" + i + "].enumedArray[" + arrayItem + "]");
}

function TestObject( arr, len ) {
  this.javaArray = arr;
  this.items    = len;
  this.enumedArray = new enumObject(arr);
}

function enumObject( o ) {
  this.pCount = 0;
  for ( var p in o ) {
    this.pCount++;
    if ( !isNaN(p) ) {
      eval( "this["+p+"] = o["+p+"]" );
    } else {
      eval( "this." + p + " = o["+ p+"]" );
    }
  }
}

