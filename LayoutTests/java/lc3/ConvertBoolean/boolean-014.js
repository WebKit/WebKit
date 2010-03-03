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

gTestfile = 'boolean-014.js';

/**
 * Preferred Argument Conversion.
 *
 * Use the syntax for explicit method invokation to override the default
 * preferred argument conversion.
 *
 */
var SECTION = "Preferred argument conversion:  boolean";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
  SECTION;
startTest();

var TEST_CLASS = applet.createQAObject("com.netscape.javascript.qa.lc3.bool.Boolean_001");

// invoke method that accepts java.lang.Boolean

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(java.lang.Boolean)\"](true)",
  "TEST_CLASS.BOOLEAN_OBJECT");

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(java.lang.Boolean)\"](false)",
  "TEST_CLASS.BOOLEAN_OBJECT");

// invoke method that expects java.lang.Object

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(java.lang.Object)\"](true)",
  "TEST_CLASS.OBJECT");

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(java.lang.Object)\"](false)",
  "TEST_CLASS.OBJECT");

// invoke method that expects a String

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(java.lang.String)\"](true)",
  "TEST_CLASS.STRING");

shouldBeWithErrorCheck(
  "TEST_CLASS[\"ambiguous(java.lang.String)\"](false)",
  "TEST_CLASS.STRING");
