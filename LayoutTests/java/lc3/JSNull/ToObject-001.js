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

gTestfile = 'ToObject-001.js';

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

a[i++] = new TestObject(
  "dt.setBooleanObject( null )",
  "dt.PUB_BOOLEAN_OBJECT",
  "dt.getBooleanObject()",
  "typeof dt.getBooleanObject()",
  'null',
  "'object'" );

a[i++] = new TestObject(
  "dt.setDoubleObject( null )",
  "dt.PUB_BOOLEAN_OBJECT",
  "dt.getDoubleObject()",
  "typeof dt.getDoubleObject()",
  'null',
  "'object'" );

a[i++] = new TestObject(
  "dt.setStringObject( null )",
  "dt.PUB_STRING",
  "dt.getStringObject()",
  "typeof dt.getStringObject()",
  'null',
  "'object'" );

for ( i = 0; i < a.length; i++ ) {
  shouldBeWithErrorCheck(
    a[i].description +"; "+ a[i].javaFieldName,
    a[i].jsValue,
    a[i].javaFieldValue );
/*
  shouldBeWithErrorCheck(
  a[i].description +"; " + a[i].javaMethodName,
  a[i].jsValue,
  a[i].javaMethodValue );
*/
  shouldBeWithErrorCheck(
    a[i].javaTypeName,
    a[i].jsType,
    a[i].javaTypeValue );

}

function TestObject( description, javaField, javaMethod, javaType,
                     jsValue, jsType )
{
  this.description = description;
  this.javaFieldName = javaField;
//    this.javaMethodName = javaMethod;
//    this.javaMethodValue = eval( javaMethod );
  this.javaTypeName = javaType,

  this.jsValue   = jsValue;
  this.jsType      = jsType;
}
