/* ***** BEGIN LICENSE BLOCK *****
 *
 * Copyright (C) 2000 Netscape Communications Corporation.
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

gTestfile = 'JavaObjectBeanProps-001.js';

var SECTION = "JavaObject Field or method access";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
  SECTION;
startTest();

var dt = applet.createQAObject("com.netscape.javascript.qa.liveconnect.DataTypeClass");

var a = [
  "boolean",
  "byte",
  "integer",
  "double",
  "float",
  "short",
  "char"
  ];

var v = [
  true,
  1,
  2,
  3.0,
  4.0,
  5,
  'a'.charCodeAt(0)
  ];

for (var i=0; i < a.length; i++) {
  var name = a[i];
  var getterName = "get" + a[i].charAt(0).toUpperCase() +
    a[i].substring(1);
  var setterName = "set" + a[i].charAt(0).toUpperCase() +
    a[i].substring(1);

  try {
    dt[name];
    dt[getterName];
  } catch (ex) {
    testFailed(ex.toString());
    continue;
  }

  shouldBeWithErrorCheck(
    'dt["' + name + '"]',
    'dt[getterName]()');

  dt[name] = v[i];
  shouldBeWithErrorCheck(
    'dt["' + getterName + '"]()',
    v[i] + "");
}

