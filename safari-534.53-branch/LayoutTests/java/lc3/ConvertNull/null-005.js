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

gTestfile = 'null-005.js';

/**
 *  Preferred Argument Conversion.
 *
 *  There is no preference among Java types for converting from the jJavaScript
 *  undefined value.
 *
 */
var SECTION = "Preferred argument conversion:  null";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
  SECTION;
startTest();

// Call an ambiguous static method using the explicit method
// syntax should succeed.

shouldBeWithErrorCheck(
  "Packages.com.netscape.javascript.qa.lc3.jsnull.Null_001[\"staticAmbiguous(java.lang.Object)\"](null)",
  "'STATIC_OBJECT'");

shouldBeWithErrorCheck(
  "Packages.com.netscape.javascript.qa.lc3.jsnull.Null_001[\"staticAmbiguous(java.lang.Boolean)\"](null)",
  "'STATIC_BOOLEAN_OBJECT'");

shouldBeWithErrorCheck(
  "Packages.com.netscape.javascript.qa.lc3.jsnull.Null_001[\"staticAmbiguous(java.lang.String)\"](null)",
  "'STATIC_STRING'");
