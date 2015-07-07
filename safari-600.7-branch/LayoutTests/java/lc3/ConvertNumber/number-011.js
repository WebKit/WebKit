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

gTestfile = 'number-011.js';

/**
 *  Preferred Argument Conversion.
 *
 *  Use the explicit method invocation syntax to override the preferred
 *  argument conversion.
 *
 */
var SECTION = "Preferred argument conversion:  undefined";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
  SECTION;
startTest();

var TEST_CLASS = applet.createQAObject("com.netscape.javascript.qa.lc3.number.Number_001");

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(java.lang.Object)\"](1)",
  "'OBJECT'");


shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(java.lang.String)\"](1)",
  "'STRING'");

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(byte)\"](1)",
  "'BYTE'");

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(char)\"](1)",
  "'CHAR'");

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(short)\"](1)",
  "'SHORT'");

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(int)\"](1)",
  "'INT'");

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(long)\"](1)",
  "'LONG'");

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(float)\"](1)",
  "'FLOAT'");

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(java.lang.Double)\"](1)",
  "'DOUBLE_OBJECT'");

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(double)\"](1)",
  "'DOUBLE'");
