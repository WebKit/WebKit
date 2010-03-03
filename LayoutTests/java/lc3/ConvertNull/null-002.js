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

gTestfile = 'null-002.js';

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

// pass null to a static method that expects an Object with explicit
// method syntax.

shouldBeWithErrorCheck(
  "java.lang.String[\"valueOf(java.lang.Object)\"](null) +''",
  "'null'");

// Pass null to a static method that expects a string without explicit
// method syntax.  In this case, there is only one matching method.

shouldBeWithErrorCheck(
  "java.lang.Boolean.valueOf(null) +''",
  "'false'");

// Pass null to a static method that expects a string  using explicit
// method syntax.

shouldBeWithErrorCheck(
  "java.lang.Boolean[\"valueOf(java.lang.String)\"](null)",
  "'false'");
