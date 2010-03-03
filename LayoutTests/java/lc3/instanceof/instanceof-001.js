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

gTestfile = 'instanceof-001.js';

/**
 *  Verify that we can use the instanceof operator on java objects.
 *
 *
 */
var SECTION = "instanceof";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
  SECTION;
startTest();


shouldBeWithErrorCheck(
  '"hi" instanceof java.lang.String',
  'false');

shouldBeWithErrorCheck(
  'new java.lang.String("hi") instanceof java.lang.String',
  'true');

shouldBeWithErrorCheck(
  'new java.lang.String("hi") instanceof java.lang.Object',
  'true');

shouldBeWithErrorCheck(
  'java.lang.String instanceof java.lang.Class',
  'false');

shouldBeWithErrorCheck(
  'java.lang.Class.forName("java.lang.String") instanceof java.lang.Class',
  'true');

shouldBeWithErrorCheck(
  'new java.lang.Double(5.0) instanceof java.lang.Double',
  'true');

shouldBeWithErrorCheck(
  'new java.lang.Double(5.0) instanceof java.lang.Number',
  'true');

shouldBeWithErrorCheck(
  'new java.lang.Double(5.0) instanceof java.lang.Double',
  'true');
