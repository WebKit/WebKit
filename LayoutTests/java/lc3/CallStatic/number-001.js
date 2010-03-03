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

gTestfile = 'number-001.js';

/**
 *  The Java language allows static methods to be invoked using either the
 *  class name or a reference to an instance of the class, but previous
 *  versions of liveocnnect only allowed the former.
 *
 *  Verify that we can call static methods and get the value of static fields
 *  from an instance reference.
 *
 *  author: christine@netscape.com
 *
 *  date:  12/9/1998
 *
 */
var SECTION = "Call static methods from an instance";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 " +
  SECTION;
startTest();

dt = applet.createQAObject("com.netscape.javascript.qa.liveconnect.DataTypeClass");

var a = new Array;
var i = 0;

a[i++] = new TestObject(
  "dt.staticSetByte( 99 )",
  "dt.PUB_STATIC_BYTE",
  "dt.staticGetByte()",
  "typeof dt.staticGetByte()",
  '99',
  '"number"' );

a[i++] = new TestObject(
  "dt.staticSetChar( 45678 )",
  "dt.PUB_STATIC_CHAR",
  "dt.staticGetChar()",
  "typeof dt.staticGetChar()",
  '45678',
  '"number"' );

a[i++] = new TestObject(
  "dt.staticSetShort( 32109 )",
  "dt.PUB_STATIC_SHORT",
  "dt.staticGetShort()",
  "typeof dt.staticGetShort()",
  '32109',
  '"number"' );

a[i++] = new TestObject(
  "dt.staticSetInteger( 2109876543 )",
  "dt.PUB_STATIC_INT",
  "dt.staticGetInteger()",
  "typeof dt.staticGetInteger()",
  '2109876543',
  '"number"' );

a[i++] = new TestObject(
  "dt.staticSetLong( 9012345678901234567 )",
  "dt.PUB_STATIC_LONG",
  "dt.staticGetLong()",
  "typeof dt.staticGetLong()",
  '9012345678901234567',
  '"number"' );

a[i++] = new TestObject(
  "dt.staticSetDouble( java.lang.Double.MIN_VALUE )",
  "dt.PUB_STATIC_DOUBLE",
  "dt.staticGetDouble()",
  "typeof dt.staticGetDouble()",
  "java.lang.Double.MIN_VALUE",
  '"number"' );

a[i++] = new TestObject(
  "dt.staticSetFloat( java.lang.Float.MIN_VALUE )",
  "dt.PUB_STATIC_FLOAT",
  "dt.staticGetFloat()",
  "typeof dt.staticGetFloat()",
  "java.lang.Float.MIN_VALUE",
  '"number"' );

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

function TestObject(description, javaField, javaMethod, javaType, jsValue, jsType)
{
  this.description = description;
  this.javaFieldName = javaField;
  this.javaMethodName = javaMethod;
  this.javaTypeName = javaType,
  this.jsValue = jsValue;
  this.jsType = jsType;
}
