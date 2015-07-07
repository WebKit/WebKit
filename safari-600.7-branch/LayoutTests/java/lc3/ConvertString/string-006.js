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

gTestfile = 'string-006.js';

/**
 *  Preferred Argument Conversion.
 *
 *  Pass a JavaScript number to ambiguous method.  Use the explicit method
 *  invokation syntax to override the preferred argument conversion.
 *
 */
var SECTION = "Preferred argument conversion: string";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
  SECTION;
startTest();

var TEST_CLASS = applet.createQAObject("com.netscape.javascript.qa.lc3.string.String_001");;

var string = "255";

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(java.lang.String)\"](string) + ''",
  "'STRING'");

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(java.lang.Object)\"](string) + ''",
  "'OBJECT'");

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(char)\"](string) + ''",
  "'CHAR'");

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(double)\"](string) + ''",
  "'DOUBLE'");

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(float)\"](string) + ''",
  "'FLOAT'");

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(long)\"](string) + ''",
  "'LONG'");

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(int)\"](string) + ''",
  "'INT'");

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(short)\"](string) + ''",
  "'SHORT'");

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(byte)\"](\"127\") + ''",
  "'BYTE'");

