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

gTestfile = 'byte-001.js';

/**
 *  java array objects "inherit" JS string methods.  verify that byte arrays
 *  can inherit JavaScript Array object methods
 *
 *
 */
var SECTION = "java array object inheritance JavaScript Array methods";
var VERSION = "1_4";
var TITLE   = "LiveConnect 3.0 " + SECTION;

startTest();

var a = new Array();

a[a.length] = new TestObject(
  "b"+a.length+" = wkTestStringToJavaByteArray(\"hello\"); b"+a.length+".join() +''",
  "b"+a.length,
  "join",
  'true',
  '"104,101,108,108,111"' );

a[a.length] = new TestObject(
  "b"+a.length+" = wkTestStringToJavaByteArray(\"JavaScript\"); b"+a.length+".reverse().join() +''",
  "b"+a.length,
  "reverse",
  'true',
  'getCharValues("tpircSavaJ")' );

a[a.length] = new TestObject(
  "b"+a.length+" = wkTestStringToJavaByteArray(\"JavaScript\"); b"+a.length+".sort().join() +''",
  "b"+a.length,
  "sort",
  'true',
  '"105,112,114,116,118,74,83,97,97,99"' );

a[a.length] = new TestObject(
  "b"+a.length+" = wkTestStringToJavaByteArray(\"JavaScript\"); b"+a.length+".sort().join() +''",
  "b"+a.length,
  "sort",
  'true',
  '"105,112,114,116,118,74,83,97,97,99"' );

// given a string, return a string consisting of the char value of each
// character in the string, separated by commas

function getCharValues(string) {
  for ( var c = 0, cString = ""; c < string.length; c++ ) {
    cString += string.charCodeAt(c) + ((c+1 < string.length) ? "," : "");
  }
  return cString;
}

// figure out what methods exist
// if there is no java method with the same name as a js method, should
// be able to invoke the js method without casting to a js string.  also
// the method should equal the same method of String.prototype.
// if there is a java method with the same name as a js method, invoking
// the method should call the java method

function TestObject( description, ob, method, override, expect ) {
  this.description = description;
  this.object = ob;
  this.method = method;
  this.override = override

  // verify result of method
  shouldBeWithErrorCheck(
    description,
    expect);

  // verify that method is the method of Array.prototype

  shouldBeWithErrorCheck(
    ob +"." + method +" == Array.prototype." + method,
    override);

  // verify that it's not cast to JS Array type

  shouldBeWithErrorCheck(
    ob + ".getClass().getName() +''",
    '"[B"');

}
