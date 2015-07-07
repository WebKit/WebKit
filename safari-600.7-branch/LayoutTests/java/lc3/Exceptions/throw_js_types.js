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

gTestfile = 'throw_js_types.js';

/**
   File Name:          throw_js_type.js
   Title:              Throw JS types as exceptions through Java
   Description:        Attempt to throw all of the basic JS primitive types
   ie.         String
   Number
   Boolean
   Object
   as exceptions through Java. If wrapping/unwrapping
   occurs as expected, the original type will be
   preserved.

   Author:             Chris Cooper
   Email:              coop@netscape.com
   Date:               12/04/1998
*/

var SECTION = "LC3";
var TITLE   = "Throw JS types as exceptions through Java";
startTest();

var evalObject = applet.createQAObject("com.netscape.javascript.qa.liveconnect.JSObjectEval");

/**************************
 * JS String
 *************************/
shouldThrow('evalObject.eval(this, "throw \'foo\';")', '"foo"');

/**************************
 * JS Number (int)
 *************************/
shouldThrow('evalObject.eval(this, "throw 42;")', '42');

/**************************
 * JS Number (float)
 *************************/
shouldThrow('evalObject.eval(this, "throw 4.2;")', '4.2');

/**************************
 * JS Boolean
 *************************/
shouldThrow('evalObject.eval(this, "throw false;")', 'false');

/**************************
 * JS Object
 *************************/
shouldThrow('evalObject.eval(this, "throw {a:5};")', 'Object.prototype.toString()');

/**************************
 * JS Object (Date)
 *************************/
try {
    evalObject.eval(this, "throw new Date();");
    testFailed('evalObject.eval(this, "throw new Date();") did not throw an exception');
} catch (ex) {
    if (ex.__proto__ == Date.prototype)
        testPassed('evalObject.eval(this, "throw new Date();") threw exception ' + ex.__proto__);
    else
        testFailed('evalObject.eval(this, "throw new Date();") should throw Date. Threw ' + ex._proto__);
}

/**************************
 * JS Object (String)
 *************************/
shouldThrow('evalObject.eval(this, "throw new String();")', 'String.prototype.toString()');

/**************************
 * Undefined
 *************************/
shouldThrow('evalObject.eval(this, "throw undefined")', 'undefined');

/**************************
 * null
 *************************/
shouldThrow('evalObject.eval(this, "throw null")', 'null');
