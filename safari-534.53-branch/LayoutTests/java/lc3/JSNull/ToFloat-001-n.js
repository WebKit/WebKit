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

gTestfile = 'ToFloat-001-n.js';

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
var SECTION = "null";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
  SECTION;
startTest();

var dt = applet.createQAObject("com.netscape.javascript.qa.liveconnect.DataTypeClass");

var a = new Array();
var i = 0;

// passing a JavaScript null to a method that expect a class or interface
// converts null to a java null.  passing null to methods that expect
// number types convert null to 0.  passing null to methods that expect
// a boolean convert null to false.

DESCRIPTION = "dt.setFloat( null )";
EXPECTED = "error";

a[i++] = new TestObject(
  "dt.setFloat( null )",
  "dt.PUB_FLOAT",
  "dt.getFloat()",
  "typeof dt.getFloat()",
  "'error'",
  "'error'" );
/*
  a[i++] = new TestObject(
  "dt.setLong( null )",
  "dt.PUB_LONG",
  "dt.getLong()",
  "typeof dt.getLong()",
  '0',
  "'number'" );

  a[i++] = new TestObject(
  "dt.setInteger( null )",
  "dt.PUB_INT",
  "dt.getInteger()",
  "typeof dt.getInteger()",
  '0',
  "'number'" );

  a[i++] = new TestObject(
  "dt.setShort( null )",
  "dt.PUB_SHORT",
  "dt.getShort()",
  "typeof dt.getShort()",
  '0',
  "'number'" );

  a[i++] = new TestObject(
  "dt.setByte( null )",
  "dt.PUB_BYTE",
  "dt.getByte()",
  "typeof dt.getByte()",
  '0',
  "'number'" );

  a[i++] = new TestObject(
  "dt.setChar( null )",
  "dt.PUB_CHAR",
  "dt.getChar()",
  "typeof dt.getChar()",
  '0',
  "'number'" );
*/

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
