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

gTestfile = 'object-001.js';

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
  "dt.staticSetDoubleObject( 99 )",
  "dt.PUB_STATIC_DOUBLE_OBJECT.doubleValue()",
  "dt.staticGetDoubleObject().doubleValue()",
  "dt.staticGetDoubleObject().getClass().getName() +''",
  '99',
  '"java.lang.Double"' );

a[i++] = new TestObject(
  "dt.staticSetShortObject( new java.lang.Short(32109) )",
  "dt.PUB_STATIC_SHORT_OBJECT.doubleValue()",
  "dt.staticGetShortObject().doubleValue()",
  "dt.staticGetShortObject().getClass().getName() +''",
  '32109',
  '"java.lang.Short"' );

a[i++] = new TestObject(
  "dt.staticSetIntegerObject( new java.lang.Integer(2109876543) )",
  "dt.PUB_STATIC_INTEGER_OBJECT.doubleValue()",
  "dt.staticGetIntegerObject().doubleValue()",
  "dt.staticGetIntegerObject().getClass().getName() +''",
  '2109876543',
  '"java.lang.Integer"' );


a[i++] = new TestObject(
  "dt.staticSetLongObject( new java.lang.Long(9012345678901234567) )",
  "dt.PUB_STATIC_LONG_OBJECT.doubleValue()",
  "dt.staticGetLongObject().doubleValue()",
  "dt.staticGetLongObject().getClass().getName() +''",
  '9012345678901234567',
  '"java.lang.Long"' );

a[i++] = new TestObject(
  "dt.staticSetDoubleObject(new java.lang.Double( java.lang.Double.MIN_VALUE) )",
  "dt.PUB_STATIC_DOUBLE_OBJECT.doubleValue()",
  "dt.staticGetDoubleObject().doubleValue()",
  "dt.staticGetDoubleObject().getClass().getName()+''",
  "java.lang.Double.MIN_VALUE",
  "java.lang.Double" );

a[i++] = new TestObject(
  "dt.staticSetFloatObject( new java.lang.Float(java.lang.Float.MIN_VALUE) )",
  "dt.PUB_STATIC_FLOAT_OBJECT.doubleValue()",
  "dt.staticGetFloatObject().doubleValue()",
  "dt.staticGetFloatObject().getClass().getName() +''",
  "java.lang.Float.MIN_VALUE",
  '"java.lang.Float"' );

a[i++] = new TestObject(
  "dt.staticSetCharacter( new java.lang.Character(45678) )",
  "dt.PUB_STATIC_CHAR_OBJECT.charValue()",
  "dt.staticGetCharacter().charValue()",
  "dt.staticGetCharacter().getClass().getName()+''",
  '45678',
  '"java.lang.Character"' );

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
