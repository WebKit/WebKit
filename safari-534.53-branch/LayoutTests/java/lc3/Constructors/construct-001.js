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

gTestfile = 'construct-001.js';

/**
 *  Verify that specific constructors can be invoked.
 *
 *
 */

var SECTION = "Explicit Constructor Invokation";
var VERSION = "JS1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Conversion";

startTest();

var DT = Packages.com.netscape.javascript.qa.liveconnect.DataTypeClass;

shouldBeWithErrorCheck(
  "new DT().PUB_INT_CONSTRUCTOR_ARG",
  "new DT().CONSTRUCTOR_ARG_NONE");

shouldBeWithErrorCheck(
  "new DT(5).PUB_INT_CONSTRUCTOR_ARG",
  "new DT(5).CONSTRUCTOR_ARG_DOUBLE");

shouldBeWithErrorCheck(
  "new DT(true).PUB_INT_CONSTRUCTOR_ARG",
  "new DT(true).CONSTRUCTOR_ARG_BOOLEAN");

// force type conversion

// convert boolean

shouldBeWithErrorCheck(
 'new DT["(boolean)"](true).PUB_INT_CONSTRUCTOR_ARG',
  'new DT("5").CONSTRUCTOR_ARG_BOOLEAN');

shouldBeWithErrorCheck(
  'new DT["(java.lang.Boolean)"](true).PUB_INT_CONSTRUCTOR_ARG',
  'new DT().CONSTRUCTOR_ARG_BOOLEAN_OBJECT');

shouldBeWithErrorCheck(
  'new DT["(java.lang.Object)"](true).PUB_INT_CONSTRUCTOR_ARG',
  'new DT("5").CONSTRUCTOR_ARG_OBJECT');

shouldBeWithErrorCheck(
  'new DT["(java.lang.String)"](true).PUB_INT_CONSTRUCTOR_ARG',
  'new DT().CONSTRUCTOR_ARG_STRING');


// convert number

shouldBeWithErrorCheck(
  'new DT["(double)"]("5").PUB_INT_CONSTRUCTOR_ARG',
  'new DT(5).CONSTRUCTOR_ARG_DOUBLE');

shouldBeWithErrorCheck(
  'new DT["(java.lang.Double)"](5).PUB_INT_CONSTRUCTOR_ARG',
  'new DT(5).CONSTRUCTOR_ARG_DOUBLE_OBJECT');

shouldBeWithErrorCheck(
  'new DT["(char)"](5).PUB_INT_CONSTRUCTOR_ARG',
  'new DT().CONSTRUCTOR_ARG_CHAR');

shouldBeWithErrorCheck(
  'new DT["(java.lang.Object)"](5).PUB_INT_CONSTRUCTOR_ARG',
  'new DT().CONSTRUCTOR_ARG_OBJECT');

shouldBeWithErrorCheck(
  'new DT["(java.lang.String)"](5).PUB_INT_CONSTRUCTOR_ARG',
  'new DT().CONSTRUCTOR_ARG_STRING');

// convert string

shouldBeWithErrorCheck(
  'new DT("5").PUB_INT_CONSTRUCTOR_ARG',
  'new DT("5").CONSTRUCTOR_ARG_STRING');

shouldBeWithErrorCheck(
  'new DT["(java.lang.String)"]("5").PUB_INT_CONSTRUCTOR_ARG',
  'new DT("5").CONSTRUCTOR_ARG_STRING');

shouldBeWithErrorCheck(
  'new DT["(char)"]("J").PUB_INT_CONSTRUCTOR_ARG',
  'new DT().CONSTRUCTOR_ARG_CHAR');

shouldBeWithErrorCheck(
  'new DT["(double)"]("5").PUB_INT_CONSTRUCTOR_ARG',
  'new DT("5").CONSTRUCTOR_ARG_DOUBLE');

shouldBeWithErrorCheck(
  'new DT["(java.lang.Object)"]("hello").PUB_INT_CONSTRUCTOR_ARG',
  'new DT("hello").CONSTRUCTOR_ARG_OBJECT');

// convert java object

shouldBeWithErrorCheck(
  'new DT["(java.lang.Object)"](new java.lang.String("hello")).PUB_INT_CONSTRUCTOR_ARG',
  'new DT().CONSTRUCTOR_ARG_OBJECT');

shouldBeWithErrorCheck(
  'new DT["(java.lang.Object)"](new java.lang.String("hello")).PUB_INT_CONSTRUCTOR_ARG',
  'new DT().CONSTRUCTOR_ARG_OBJECT');

shouldBeWithErrorCheck(
  'new DT["(double)"](new java.lang.Double(5)).PUB_INT_CONSTRUCTOR_ARG',
  'new DT().CONSTRUCTOR_ARG_DOUBLE');

shouldBeWithErrorCheck(
  'new DT["(char)"](new java.lang.Double(5)).PUB_INT_CONSTRUCTOR_ARG',
  'new DT().CONSTRUCTOR_ARG_CHAR');

shouldBeWithErrorCheck(
  'new DT["(java.lang.String)"](new java.lang.Double(5)).PUB_INT_CONSTRUCTOR_ARG',
  'new DT().CONSTRUCTOR_ARG_STRING');

shouldBeWithErrorCheck(
  'new DT["(java.lang.Double)"](new java.lang.Double(5)).PUB_INT_CONSTRUCTOR_ARG',
  new DT().CONSTRUCTOR_ARG_DOUBLE_OBJECT);

shouldBeWithErrorCheck(
  'new DT(new java.lang.Double(5)).PUB_INT_CONSTRUCTOR_ARG',
  new DT().CONSTRUCTOR_ARG_DOUBLE_OBJECT);

// java array

shouldBeWithErrorCheck(
  'new DT["(java.lang.String)"](new java.lang.String("hello").getBytes()).PUB_INT_CONSTRUCTOR_ARG',
  'new DT("hello").CONSTRUCTOR_ARG_STRING');

shouldBeWithErrorCheck(
  'new DT["(byte[])"](new java.lang.String("hello").getBytes()).PUB_INT_CONSTRUCTOR_ARG',
  'new DT("hello").CONSTRUCTOR_ARG_BYTE_ARRAY');
