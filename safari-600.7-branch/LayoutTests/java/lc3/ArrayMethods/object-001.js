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
 * Boston, MA 02110-1301, USA.<!DOCTYPE html>
 *
 * ***** END LICENSE BLOCK ***** */

gTestfile = 'object-001.js';

/**
 *  java array objects "inherit" JS string methods.  verify that byte arrays
 *  can inherit JavaScript Array object methods join, reverse, sort and valueOf
 *
 */
var SECTION = "java array object inheritance JavaScript Array methods";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 " + SECTION;

startTest();

dt = applet.createQAObject("com.netscape.javascript.qa.liveconnect.DataTypeClass");

obArray = dt.PUB_ARRAY_OBJECT;

// check string value

shouldBeWithErrorCheck(
  "obArray.join()",
  'join(obArray)');

// check type of object returned by method

shouldBeWithErrorCheck(
  "obArray.reverse().join() + ''",
  'reverse(obArray)');

shouldBeWithErrorCheck(
  "obArray.reverse().getClass().getName() +''",
  '"[Ljava.lang.Object;"');

function join( a ) {
  for ( var i = 0, s = ""; i < a.length; i++ ) {
    s += a[i].toString() + ( i + 1 < a.length ? "," : "" );
  }
  return s;
}
function reverse( a ) {
  for ( var i = a.length -1, s = ""; i >= 0; i-- ) {
    s += a[i].toString() + ( i> 0 ? "," : "" );
  }
  return s;
}
