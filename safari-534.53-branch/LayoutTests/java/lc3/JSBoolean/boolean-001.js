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

gTestfile = 'boolean-001.js';

/* -*- Mode: java; tab-width: 8 -*-
 * Copyright (C) 1997, 1998 Netscape Communications Corporation,
 * All Rights Reserved.
 */

/**
 *  JavaScript to Java type conversion.
 *
 *  This test passes JavaScript boolean values to several Java methods
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
var SECTION = "boolean conversion";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
  SECTION;
var BUGNUMBER = "335907";

startTest();

var dt = applet.createQAObject("com.netscape.javascript.qa.liveconnect.DataTypeClass");

var a = new Array();
var i = 0;

// passing a boolean to a java method that expects a boolean should map
// directly to the Java true / false equivalent

a[i++] = new TestObject(
  "dt.setBoolean( true )",
  "dt.PUB_BOOLEAN",
  "dt.getBoolean()",
  "typeof dt.getBoolean()",
  'true',
  '"boolean"' );

a[i++] = new TestObject(
  "dt.setBoolean( false )",
  "dt.PUB_BOOLEAN",
  "dt.getBoolean()",
  "typeof dt.getBoolean()",
  'false',
  '"boolean"' );

// passing a boolean to a java method that expects a Boolean object
// should convert to a new instance of java.lang.Boolean

a[i++] = new TestObject(
  "dt.setBooleanObject( true )",
  "dt.PUB_BOOLEAN_OBJECT +''",
  "dt.getBooleanObject() +''",
  "dt.getBooleanObject().getClass()",
  '"true"',
  'java.lang.Class.forName( "java.lang.Boolean")' );

a[i++] = new TestObject(
  "dt.setBooleanObject( false )",
  "dt.PUB_BOOLEAN_OBJECT +''",
  "dt.getBooleanObject() +''",
  "dt.getBooleanObject().getClass()",
  '"false"',
  'java.lang.Class.forName( "java.lang.Boolean")' );


// passing a boolean to a java method that expects a java.lang.Object
// should convert to a new instance of java.lang.Boolean

a[i++] = new TestObject(
  "dt.setObject( true )",
  "dt.PUB_OBJECT +''",
  "dt.getObject() +''",
  "dt.getObject().getClass()",
  '"true"',
  'java.lang.Class.forName( "java.lang.Boolean")' );

a[i++] = new TestObject(
  "dt.setObject( false )",
  "dt.PUB_OBJECT +''",
  "dt.getObject() +''",
  "dt.getObject().getClass()",
  '"false"',
  'java.lang.Class.forName( "java.lang.Boolean")' );

// passing a boolean to a java method that expects a java.lang.String
// should convert true to a java.lang.String whose value is "true" and
// false to a java.lang.String whose value is "false"

a[i++] = new TestObject(
  "dt.setStringObject( true )",
  "dt.PUB_STRING +''",
  "dt.getStringObject() +''",
  "dt.getStringObject().getClass()",
  '"true"',
  'java.lang.Class.forName( "java.lang.String")' );

a[i++] = new TestObject(
  "dt.setStringObject( false )",
  "dt.PUB_STRING +''",
  "dt.getStringObject() +''",
  "dt.getStringObject().getClass()",
  '"false"',
  'java.lang.Class.forName( "java.lang.String")' );

for ( i = 0; i < a.length; i++ ) {
  shouldBeWithErrorCheck(
    a[i].description +"; "+ a[i].javaFieldName,
    a[i].jsValue);

  shouldBeWithErrorCheck(
    a[i].description +"; " + a[i].javaMethodName,
    a[i].jsValue);

  try {
    eval(a[i].jsType);
  } catch (ex) {
    testFailed(a[i].jsType + ": " + ex);
    continue;
  }

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
